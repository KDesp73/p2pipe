#include "p2pipe/pipe.h"
#include "extern/logging.h"
#include "futils.h"
#include "p2pipe/metrics.h"
#include "p2pipe/packet.h"
#include "p2pipe/storage.h"
#include "p2pipe/udp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void pipe_init(Pipe* pipe, size_t capacity)
{
    pipe->buffer = malloc(sizeof(Packet) * capacity);
    if(!pipe->buffer) {
        ERRO("Could not allocate buffer");
        return;
    }
    pipe->capacity = capacity;
    pipe->count = 0;
}

static int pipe_handshake(Pipe* pipe, const char* ip, size_t port, size_t capacity, PipeMode mode)
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

        struct sockaddr_in srv = {0};
        srv.sin_family = AF_INET;
        srv.sin_port = htons(port);
        inet_pton(AF_INET, ip, &srv.sin_addr);

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
                info.id = strdup(token + 3);  // NOTE: this pointer is being freed by metrics_free()
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

int pipe_rcv_open(Pipe* pipe, const char* ip, size_t port, size_t capacity)
{
    if (!pipe || !ip) return -1;

    pipe_init(pipe, capacity);
    pipe->mode = MODE_RCV;

    return pipe_handshake(pipe, ip, port, capacity, MODE_RCV);
}

bool pipe_read(Pipe* pipe, size_t n_bytes, const char* dst)
{
    if (!pipe || pipe->mode != MODE_RCV || pipe->sock_fd < 0)
        return false;

    size_t total_read = 0;
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    while (total_read < n_bytes) {
        uint8_t buf[PACKET_BUFFER_SIZE + 9]; // 1 + 4 + 4 + data
        ssize_t bytes = recvfrom(pipe->sock_fd,
                                 buf,
                                 sizeof(buf),
                                 0,
                                 (struct sockaddr*)&src_addr,
                                 &addrlen);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            perror("recvfrom");
            return false;
        }

        if (bytes == 0)
            break;

        metrics.packets_received++;

        Packet* packet = &pipe->buffer[pipe->count % pipe->capacity];
        packet_deserialize(packet, buf, (size_t)bytes);

        if(packet->signals & SIGNAL_END) {
            INFO("Received END signal");
            return true;
        }

        storage_append(&storage, packet);

        pipe->count++;
        total_read += packet->len;

        INFO("Received packet #%u: %u bytes from %s:%d",
            packet->seq,
            (unsigned)packet->len,
            inet_ntoa(src_addr.sin_addr),
            ntohs(src_addr.sin_port));
    }

    return total_read > 0;
}

void pipe_rcv_close(Pipe* pipe)
{
    if (!pipe) return;

    if (pipe->sock_fd >= 0) {
        close(pipe->sock_fd);
        pipe->sock_fd = -1;
    }

    if (pipe->buffer) {
        free(pipe->buffer);
        pipe->buffer = NULL;
    }

    pipe->count = 0;
    pipe->capacity = 0;

    printf("[INFO] Pipe receiver closed\n");
}

int pipe_snd_open(Pipe* pipe, const char* ip, size_t port, size_t capacity)
{
    pipe_init(pipe, capacity);
    pipe->mode = MODE_SND;

    return pipe_handshake(pipe, ip, port, capacity, MODE_SND);
}

static bool pipe_write_packet(Pipe* pipe, const Packet* packet)
{
    if (!pipe || !packet) return false;

    uint8_t buf[sizeof(Packet)];
    size_t len = packet_serialize(packet, buf, sizeof(buf));
    if (len == 0) return false;


    ssize_t sent = sendto(pipe->sock_fd, buf, len, 0,
                          (struct sockaddr*)&pipe->peer_addr,
                          sizeof(pipe->peer_addr));
    metrics.packets_sent++;
    return sent == len;
}

bool pipe_write(Pipe* pipe, void* payload, size_t len)
{
    if (!pipe || pipe->mode != MODE_SND || pipe->sock_fd < 0)
        return false;

    uint8_t* data = payload;
    size_t offset = 0;
    uint32_t seq = 0;

    while (offset < len) {
        Packet packet = {0};
        packet.signals = SIGNAL_PAYLOAD;
        packet.seq = seq++;
        packet.len = (len - offset) < PACKET_BUFFER_SIZE ? (len - offset) : PACKET_BUFFER_SIZE;
        memcpy(packet.data, data + offset, packet.len);

        pipe->buffer[pipe->count++] = packet;

        uint8_t buf[sizeof(Packet)];
        size_t buf_len = packet_serialize(&packet, buf, sizeof(buf));
        if (buf_len == 0) return false;

        ssize_t sent = sendto(pipe->sock_fd, buf, buf_len, 0,
                              (struct sockaddr*)&pipe->peer_addr,
                              sizeof(pipe->peer_addr));
        if (sent != buf_len) {
            perror("sendto");
            return false;
        }

        metrics.packets_sent++;

        INFO("Sent packet #%u: %u bytes to %s:%d", packet.seq, packet.len, 
                inet_ntoa(pipe->peer_addr.sin_addr),
                ntohs(pipe->peer_addr.sin_port));

        offset += packet.len;
        pipe->count++;
    }

    return true;
}

bool pipe_flush(Pipe* pipe)
{
    (void)pipe; // unused
    return true;
}

void pipe_snd_close(Pipe* pipe)
{
    if (!pipe) return;

    if(!pipe_write_packet(pipe, &PACKET_END)) {
        WARN("END signal could was not send. Communication might have already ended");
    }

    // Cleanup

    if (pipe->sock_fd >= 0) {
        close(pipe->sock_fd);
        pipe->sock_fd = -1;
    }

    if (pipe->buffer) {
        free(pipe->buffer);
        pipe->buffer = NULL;
    }

    pipe->count = 0;
    pipe->capacity = 0;
    INFO("Pipe sender closed");
}
