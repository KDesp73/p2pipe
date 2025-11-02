#include "p2pipe/udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>

ssize_t udp_handshake_blocking(const char *server_ip,
        unsigned short server_port,
        const char *payload,
        char *out_buf,
        size_t out_buf_size,
        unsigned timeout_sec,
        unsigned max_retries,
        unsigned short client_port,
        int *out_sock)
{
    if (!server_ip || !payload || !out_buf || out_buf_size == 0) {
        errno = EINVAL;
        return -1;
    }

    char msg[1024];
    int needed = snprintf(msg, sizeof(msg), "HANDSHAKE %s", payload);
    if (needed < 0 || (size_t)needed >= sizeof(msg)) {
        errno = EMSGSIZE;
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(client_port);
    if (bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        errno = EADDRNOTAVAIL;
        return -1;
    }

    unsigned tries = 0;
    for (;;) {
        ssize_t sent = sendto(sock, msg, (size_t)needed, 0,
                              (struct sockaddr*)&srv, sizeof(srv));
        if (sent < 0) {
            close(sock);
            return -1;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        struct timeval tv = { .tv_sec = timeout_sec, .tv_usec = 0 };

        int rv = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(sock, &rfds)) {
            struct sockaddr_storage from;
            socklen_t fromlen = sizeof(from);
            ssize_t n = recvfrom(sock, out_buf, out_buf_size - 1, 0,
                                 (struct sockaddr*)&from, &fromlen);
            if (n < 0) {
                if (errno == EINTR) continue;
                close(sock);
                return -1;
            }
            out_buf[n] = '\0';
            if (out_sock) *out_sock = sock;
            return n;
        } else if (rv == 0) {
            tries++;
            if (max_retries != 0 && tries >= max_retries) {
                close(sock);
                errno = ETIMEDOUT;
                return -1;
            }
            continue;
        } else {
            if (errno == EINTR) continue;
            close(sock);
            return -1;
        }
    }
}

unsigned short get_available_udp_port(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(0);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 0;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sock, (struct sockaddr*)&addr, &len) < 0) {
        perror("getsockname");
        close(sock);
        return 0;
    }

    unsigned short port = ntohs(addr.sin_port);
    close(sock);
    return port;
}
