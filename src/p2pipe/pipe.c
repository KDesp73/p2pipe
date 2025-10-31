#include "p2pipe/pipe.h"
#include "extern/logging.h"
#include "p2pipe/udp.h"
#include <arpa/inet.h>
#include <errno.h>
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
}

static int pipe_handshake(Pipe* pipe, const char* ip, size_t port, size_t capacity)
{
    char payload[512];
    snprintf(payload, sizeof(payload), "BUF=%zu PROTO=1", capacity);

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

            struct timeval tv = { .tv_sec = 15, .tv_usec = 0 }; // wait up to 15 seconds
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

    return pipe_handshake(pipe, ip, port, capacity);
}

bool pipe_read(Pipe* pipe, size_t n)
{
    if (!pipe || pipe->mode != MODE_RCV || pipe->sock_fd < 0)
        return false;

    if (n > pipe->capacity)
        n = pipe->capacity;

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    ssize_t bytes = recvfrom(pipe->sock_fd,
                             pipe->buffer[pipe->count % pipe->capacity],
                             PACKET_SIZE, 0,
                             (struct sockaddr*)&src_addr, &addrlen);

    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;
        perror("recvfrom");
        return false;
    }

    pipe->count++;

    printf("[INFO] Received %zd bytes from %s:%d\n",
           bytes,
           inet_ntoa(src_addr.sin_addr),
           ntohs(src_addr.sin_port));

    return true;
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

    return pipe_handshake(pipe, ip, port, capacity);
}

bool pipe_write(Pipe* pipe, Packet packet)
{
    if (!pipe || pipe->mode != MODE_SND || pipe->sock_fd < 0)
        return false;

    ssize_t sent = sendto(pipe->sock_fd,
                          packet,
                          PACKET_SIZE,
                          0,
                          (struct sockaddr*)&pipe->peer_addr,
                          sizeof(pipe->peer_addr));

    if (sent != PACKET_SIZE) {
        perror("sendto");
        return false;
    }

    pipe->count++;
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
    printf("[INFO] Pipe sender closed\n");
}
