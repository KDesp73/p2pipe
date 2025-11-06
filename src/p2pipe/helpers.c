#include <errno.h>
#include <pthread.h>
#include "extern/logging.h"
#include "p2pipe/handshake.h"
#include "p2pipe/metrics.h"
#include "p2pipe/packet.h"
#include "p2pipe/pipe.h"
#include "p2pipe/threads.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "p2pipe/helpers.h"

#define RETRANSMISSION_TIMEOUT_MS 500

long long current_time_ms()
{
    struct timeval te; 
    gettimeofday(&te, NULL);
    return te.tv_sec*1000LL + te.tv_usec/1000;
}

void retransmission_thread(void* arg)
{
    Pipe* pipe = (Pipe*)arg;

    pipe->retransmit_running = true;
    INFO("Retransmission thread started.");

    while (pipe->retransmit_running) {
        usleep(RETRANSMISSION_TIMEOUT_MS * 1000 / 2);

        pthread_mutex_lock(&pipe->ack_lock);

        if (pipe->buffer.count > 0) {
            long long now = current_time_ms();
            size_t retransmitted_count = 0;
            
            for (size_t i = 0; i < pipe->buffer.capacity; i++) {
                Packet* pkt = &pipe->buffer.items[i];
                if (pkt != NULL && (now - pkt->last_sent_ms > RETRANSMISSION_TIMEOUT_MS)) {
                    INFO("Retransmitting packet #%u (Timeout: %lldms).", pkt->seq, now - pkt->last_sent_ms);
                    if (pipe_write_packet_async(pipe, pkt, NULL)) {
                        pkt->last_sent_ms = now;
                        retransmitted_count++;
                    }
                }
            }
            
            if(retransmitted_count > 0) {
                WARN("Retransmitted %zu packets.", retransmitted_count);
            }
        }

        pthread_mutex_unlock(&pipe->ack_lock);
    }
    
    INFO("Retransmission thread stopped.");
}

bool threads_init(void)
{
    if (tp && oe) return true;

    tp = thread_pool_create(16);
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
    }

    if (tp) {
        thread_pool_destroy(tp);
        tp = NULL;
    }

    if (oe) {
        ordered_executor_destroy(oe);
        oe = NULL;
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
                       (packet->signals & SIGNAL_ACK) ? "ACK" : 
                       (packet->signals & SIGNAL_HANDSHAKE) ? "HANDSHAKE" : "DATA";

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
    pthread_cond_destroy(&pipe->storage_cond);
    pthread_mutex_destroy(&pipe->storage_lock);

    threads_shutdown();
}

static void process_ack(Pipe* pipe, const Packet* packet)
{
    pthread_mutex_lock(&pipe->ack_lock);
    if (buffer_remove(&pipe->buffer, packet->seq)) {
        pthread_cond_signal(&pipe->ack_cond);
        pthread_cond_broadcast(&pipe->ack_cond);
        INFO("Successfully processed ACK #%u. Signaling waiting sender.", packet->seq);
    } else {
        WARN("Received duplicate/stale ACK #%u. Not found in sender buffer.", packet->seq);
    }
    pthread_mutex_unlock(&pipe->ack_lock);
}

static void process_handshake(Pipe* pipe, const Packet* packet)
{
    INFO("Received HANDSHAKE signal");

    if(!(packet->signals & SIGNAL_PAYLOAD)) {
        WARN("Payload signal is not set");
        if(packet->len == 0) return;
    }

    Handshake handshake = {0};
    if(!handshake_deserialize(&handshake, packet->data, packet->len)) {
        ERRO("Failed to deserialize handshake");
        return;
    }
    handshake_print(&handshake);

    pipe->payload_len = handshake.payload_len;
    pipe->hash = handshake.hash;
    pipe->handshake_completed = true;

    INFO("Handshake completed");
}

void packet_listener(void* arg)
{
    Pipe* pipe = arg;
    if (!pipe) return;

    INFO("Packet listener thread started.");

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    uint8_t buf[PACKET_BUFFER_SIZE + 9]; // header + data

    while (pipe->running) { 
        if(pipe->sock_fd <= 0) continue;
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
            INFO("Received END signal");
            free(pkt_copy);
            pipe->end_received = true;
            continue;
        }         

        if (pkt_copy->signals & SIGNAL_HANDSHAKE) {
            process_handshake(pipe, pkt_copy);
            free(pkt_copy);
            continue;
        }

        if (pkt_copy->signals & SIGNAL_ACK) {
            process_ack(pipe, pkt_copy);
            free(pkt_copy);
            continue;
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
            continue;
        }

        INFO("Queued data packet #%u (%u bytes) from %s:%d.",
             (unsigned)pkt_copy->seq,
             (unsigned)pkt_copy->len,
             inet_ntoa(src_addr.sin_addr),
             ntohs(src_addr.sin_port));
    }

    INFO("Packet listener thread stopped.");
}

uint64_t compute_fnv1a_hash(const void* payload, size_t len)
{
    if (payload == NULL || len == 0) {
        return 0;
    }

    const uint64_t FNV_PRIME = 1099511628211ULL;
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    const uint8_t* data = (const uint8_t*)payload;
    
    for (size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}
