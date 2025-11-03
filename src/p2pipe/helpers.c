#include <errno.h>
#include <pthread.h>
#include "extern/logging.h"
#include "p2pipe/metrics.h"
#include "p2pipe/pipe.h"
#include "p2pipe/threads.h"
#include "p2pipe/udp.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "p2pipe/helpers.h"

int pipe_handshake(Pipe* pipe, const char* ip, size_t port, size_t capacity, PipeMode mode)
{
    char payload[512];
    snprintf(payload, sizeof(payload), "BUF=%zu PROTO=1 TYPE=%s", capacity, mode == MODE_SND ? "SND" : "RCV");

    char buf[1024];
    ssize_t n;
    int sock = -1;
    unsigned short local_port = get_available_udp_port();

    n = udp_handshake_blocking(ip, port, payload, buf, sizeof(buf),
                               3, 5, local_port, &sock);
    if (n <= 0) {
        ERRO("Initial handshake failed or timed out");
        if (sock >= 0) close(sock);
        return -1;
    }

    buf[n] = '\0';
    if (buf[n - 1] == '\n') buf[n - 1] = '\0';


    if (strcmp(buf, "WAIT") == 0) {
        INFO("Server replied WAIT — waiting for peer...");

        for (;;) {
            struct sockaddr_storage from;
            socklen_t fromlen = sizeof(from);

            struct timeval tv = { .tv_sec = 60, .tv_usec = 0 };
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            int rv = select(sock + 1, &rfds, NULL, NULL, &tv);
            if (rv > 0 && FD_ISSET(sock, &rfds)) {
                ssize_t r = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                     (struct sockaddr*)&from, &fromlen);
                if (r <= 0) continue;
                buf[r] = '\0';
                if (strncmp(buf, "PEER ", 5) == 0) break;
            } else if (rv == 0) {
                ERRO("Timeout while waiting for peer info");
                close(sock);
                return -1;
            } else {
                perror("select");
                close(sock);
                return -1;
            }
        }
    }

    if (strncmp(buf, "PEER ", 5) == 0) {
        char peer_ip[INET_ADDRSTRLEN] = {0};
        int peer_port = 0;
        char peer_info[512] = {0};

        int parsed = sscanf(buf + 5, "%15s %d %511[^\n]", peer_ip, &peer_port, peer_info);
        if (parsed < 2) {
            ERRO("Malformed PEER line: %s", buf);
            close(sock);
            return -1;
        }

        INFO("Peer info: %s:%d, extra info: %s",
             peer_ip, peer_port,
             (parsed == 3) ? peer_info : "(none)");

        HandshakeInfo info = {0};
        char* token = strtok(peer_info, " ");
        while (token) {
            if (strncmp(token, "BUF=", 4) == 0) {
                info.buf = strtoul(token + 4, NULL, 10);
            } else if (strncmp(token, "PROTO=", 6) == 0) {
                info.proto = strtoul(token + 6, NULL, 10);
            } else if (strncmp(token, "ID=", 3) == 0) {
                info.id = strdup(token + 3);
            }
            token = strtok(NULL, " ");
        }

        metrics.id = info.id;

        struct sockaddr_in peer_addr = {0};
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(peer_port);
        if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
            ERRO("Invalid peer IP: %s", peer_ip);
            close(sock);
            return -1;
        }

        pipe->sock_fd = sock;
        pipe->peer_addr = peer_addr;

        INFO("Handshake completed — connected to peer %s:%d", peer_ip, peer_port);
        return sock;
    }

    ERRO("Unknown handshake response: %s", buf);
    close(sock);
    return -1;
}

bool threads_init(void)
{
    if (tp && oe) return true;

    tp = thread_pool_create(4);
    if (!tp) return false;

    oe = ordered_executor_create(tp, 64);
    if (!oe) {
        thread_pool_destroy(tp);
        tp = NULL;
        return false;
    }

    return true;
}

void threads_shutdown(void)
{
    if (oe) {
        ordered_executor_shutdown(oe);
        ordered_executor_destroy(oe);
        oe = NULL;
    }

    if (tp) {
        thread_pool_destroy(tp);
        tp = NULL;
    }
}

void send_job_fn(void *arg)
{
    SendJob *job = (SendJob*)arg;
    if (!job) return;

    uint8_t buf[sizeof(Packet)];
    size_t len = packet_serialize(&job->packet, buf, sizeof(buf));
    if (len == 0) {
        ERRO("Failed to serialize packet (Type: %s, Seq: %u).",
             (job->packet.signals & SIGNAL_ACK) ? "ACK" : "DATA/END", job->packet.seq);
        free(job);
        return;
    }

    ssize_t sent = sendto(job->sock_fd, buf, len, 0,
                          (struct sockaddr*)&job->peer, sizeof(job->peer));
    
    const char *type = (job->packet.signals & SIGNAL_ACK) ? "ACK" : 
                       (job->packet.signals & SIGNAL_END) ? "END" : "DATA";
                       
    if (sent != (ssize_t)len) {
        WARN("Failed to send %s #%u (%zu bytes). Error: %s",
             type, job->packet.seq, len, strerror(errno));
        // TODO: implement retry logic / queueing
    } else {
        metrics.packets_sent++;
        INFO("%s packet #%u (%zu bytes) sent successfully to peer.",
             type, job->packet.seq, sent);
    }

    free(job);
}

void recv_job_fn(void *arg)
{
    RecvJob *r = (RecvJob*)arg;
    if (!r) return;

    Pipe *pipe = r->pipe;
    Packet *packet = r->packet;
    struct sockaddr_in src = r->src;

    if (!pipe || !packet) {
        free(packet);
        free(r);
        return;
    }

    if (pipe->mode == MODE_RCV) {
        pthread_mutex_lock(&pipe->storage_lock);
        
        if (pipe->storage.ready) { 
            storage_append(&pipe->storage, packet);
            
            pthread_cond_signal(&pipe->storage_cond);
            INFO("Payload packet #%u (%u bytes) processed and added to storage (Count: %zu).",
                 packet->seq, packet->len, pipe->storage.count);
        } else {
            WARN("Dropping packet #%u. Storage not ready for data delivery.", packet->seq);
        }

        pthread_mutex_unlock(&pipe->storage_lock);

        Packet ack = PACKET_ACK(packet->seq);
        SendJob *sj = malloc(sizeof(*sj));
        if (sj) {
            sj->sock_fd = pipe->sock_fd;
            sj->peer = src;
            memcpy(&sj->packet, &ack, sizeof(Packet));
            sj->pipe = pipe;
            if (thread_pool_submit(tp, send_job_fn, sj)) {
                 INFO("ACK #%u queued for sequence #%u.", ack.seq, packet->seq);
            } else {
                 WARN("Failed to queue ACK #%u for sequence #%u.", ack.seq, packet->seq);
                 free(sj);
            }
        } else {
            ERRO("Failed to allocate memory for ACK send job for packet #%u.", packet->seq);
        }
    }

    if (pipe->mode == MODE_SND) {
        if (packet->signals & SIGNAL_ACK) {
            INFO("ACK #%u received. Sender buffer size before removal: %zu.", packet->seq, pipe->buffer.count);

            pthread_mutex_lock(&pipe->ack_lock);
            if (buffer_remove(&pipe->buffer, packet->seq)) {
                pthread_cond_signal(&pipe->ack_cond); 
                metrics.packets_acked++;
                INFO("Successfully removed acknowledged packet #%u. Remaining: %zu.", 
                     packet->seq, pipe->buffer.count);
            } else {
                INFO("Received duplicate/stale ACK #%u. Not found in sender buffer.", packet->seq);
            }
            pthread_mutex_unlock(&pipe->ack_lock);
        }
    }

    free(packet);
    free(r);
}


bool pipe_write_packet_sync(Pipe* pipe, const Packet* packet, struct sockaddr_in *dest)
{
    if (!pipe || !packet) return false;
    uint8_t buf[sizeof(Packet)];
    size_t len = packet_serialize(packet, buf, sizeof(buf));
    if (len == 0) {
        ERRO("Failed to serialize packet (Seq: %u) for synchronous send.", packet->seq);
        return false;
    }

    struct sockaddr_in to = dest ? *dest : pipe->peer_addr;
    ssize_t sent = sendto(pipe->sock_fd, buf, len, 0, (struct sockaddr*)&to, sizeof(to));
    
    const char *type = (packet->signals & SIGNAL_END) ? "END" : 
                       ((packet->signals & SIGNAL_ACK) ? "ACK" : "DATA");

    if (sent != (ssize_t)len) {
        WARN("Failed to send %s packet #%u (%zu bytes). Error: %s", 
             type, packet->seq, len, strerror(errno));
        return false;
    }
    metrics.packets_sent++;
    INFO("%s packet #%u (%zu bytes) sent successfully.", 
         type, packet->seq, len);
    return true;
}

bool pipe_write_packet_async(Pipe* pipe, const Packet* packet, struct sockaddr_in *dest)
{
    if (!tp) {
        return pipe_write_packet_sync(pipe, packet, dest);
    }

    SendJob *sj = malloc(sizeof(*sj));
    if (!sj) return false;

    sj->sock_fd = pipe->sock_fd;
    if (dest) sj->peer = *dest;
    else sj->peer = pipe->peer_addr;
    memcpy(&sj->packet, packet, sizeof(Packet));
    sj->pipe = pipe;

    if (!thread_pool_submit(tp, send_job_fn, sj)) {
        free(sj);
        return false;
    }
    return true;
}

void pipe_init(Pipe* pipe, size_t capacity)
{
    pipe->sock_fd = -1;
    pipe->storage.packets = NULL;
    pipe->storage.capacity = 0;
    pipe->storage.count = 0;
    pipe->storage.ready = false;
    pipe->seq = 0;

    if(!buffer_init(&pipe->buffer, capacity)){
        ERRO("Could not initialize buffer");
        return;
    }

    if (pthread_mutex_init(&pipe->ack_lock, NULL) != 0) {
        ERRO("Failed to init mutex");
        buffer_free(&pipe->buffer);
        return;
    }
    if (pthread_cond_init(&pipe->ack_cond, NULL) != 0) {
        ERRO("Failed to init condition variable");
        pthread_mutex_destroy(&pipe->ack_lock);
        buffer_free(&pipe->buffer);
        return;
    }

    if (pthread_mutex_init(&pipe->storage_lock, NULL) != 0) {
        ERRO("Failed to init mutex");
        buffer_free(&pipe->buffer);
        return;
    }
    if (pthread_cond_init(&pipe->storage_cond, NULL) != 0) {
        ERRO("Failed to init condition variable");
        pthread_mutex_destroy(&pipe->ack_lock);
        buffer_free(&pipe->buffer);
        return;
    }

    if (!threads_init()) {
        ERRO("Failed to init threads");
        pthread_cond_destroy(&pipe->ack_cond);
        pthread_mutex_destroy(&pipe->ack_lock);
        buffer_free(&pipe->buffer);
        return;
    }
}

void pipe_free(Pipe* pipe)
{
    if (!pipe) return;

    if (pipe->sock_fd >= 0) {
        close(pipe->sock_fd);
        pipe->sock_fd = -1;
    }

    buffer_free(&pipe->buffer);
    storage_free(&pipe->storage);

    pthread_cond_destroy(&pipe->ack_cond);
    pthread_mutex_destroy(&pipe->ack_lock);

    threads_shutdown();
}

void ack_listener(void* arg)
{
    Pipe* pipe = arg;
    uint8_t buf[sizeof(Packet)];
    struct sockaddr_in src;
    socklen_t addrlen = sizeof(src);

    while (pipe->running) {
        Packet ack;
        ssize_t r = recvfrom(pipe->sock_fd, buf, sizeof(buf), 0,
                             (struct sockaddr*)&src, &addrlen);
        if (r <= 0) continue;

        if (!packet_deserialize(&ack, buf, (size_t)r)) {
            WARN("Failed to deserialize incoming packet (Size: %zd).", r);
            continue;
        }

        if (ack.signals & SIGNAL_ACK) {
            pthread_mutex_lock(&pipe->ack_lock);
            if (buffer_remove(&pipe->buffer, ack.seq)) {
                pthread_cond_signal(&pipe->ack_cond);
                pthread_cond_broadcast(&pipe->ack_cond);
                INFO("Successfully processed ACK #%u. Signaling waiting sender.", ack.seq);
            } else {
                 INFO("Received duplicate/stale ACK #%u. Not found in sender buffer.", ack.seq);
            }
            pthread_mutex_unlock(&pipe->ack_lock);
        }
    }
}

void packet_listener(void* arg)
{
    Pipe* pipe = arg;
    if (!pipe || pipe->mode != MODE_RCV || pipe->sock_fd < 0)
        return;

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    uint8_t buf[PACKET_BUFFER_SIZE + 9]; // header + data

    while (pipe->running) { 
        ssize_t bytes = recvfrom(pipe->sock_fd, buf, sizeof(buf), 0,
                                 (struct sockaddr*)&src_addr, &addrlen);

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; 
            }
            ERRO("Critical recvfrom error: %s", strerror(errno));
            pipe->running = false;
            return;
        }
        if (bytes == 0) continue;

        metrics.packets_received++;

        Packet *pkt_copy = malloc(sizeof(Packet)); 
        if (!pkt_copy) {
            ERRO("Memory allocation failed for packet copy.");
            continue; 
        }
        if (!packet_deserialize(pkt_copy, (const uint8_t*)buf, (size_t)bytes)) {
            WARN("Failed to deserialize incoming packet (Size: %zd). Dropping.", bytes);
            free(pkt_copy);
            continue;
        }

        if (pkt_copy->signals & SIGNAL_END) {
            INFO("Received END signal. Shutting down listener and notifying readers.");
            free(pkt_copy);
            pthread_mutex_lock(&pipe->storage_lock);
            pipe->running = false;
            pthread_cond_broadcast(&pipe->storage_cond);
            pthread_mutex_unlock(&pipe->storage_lock);
            return; 
        }
        
        RecvJob *job = malloc(sizeof(*job));
        if (!job) { 
            ERRO("Memory allocation failed for RecvJob.");
            free(pkt_copy); 
            continue; 
        }
        job->pipe = pipe;
        job->packet = pkt_copy;
        job->src = src_addr;

        uint64_t key = (((uint64_t)ntohl(src_addr.sin_addr.s_addr)) << 16) ^ (uint64_t)ntohs(src_addr.sin_port);

        if (!ordered_executor_submit(oe, key, recv_job_fn, job)) {
            WARN("Failed to submit RecvJob for packet #%u to OE. Running inline.", (unsigned)pkt_copy->seq);
            recv_job_fn(job);
        }

        INFO("Queued data packet #%u (%u bytes) from %s:%d.",
             (unsigned)pkt_copy->seq,
             (unsigned)pkt_copy->len,
             inet_ntoa(src_addr.sin_addr),
             ntohs(src_addr.sin_port));
    }
}
