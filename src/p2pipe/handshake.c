#include "p2pipe/handshake.h"
#include "p2pipe/helpers.h"
#include "p2pipe/id.h"
#include "p2pipe/metrics.h"
#include "extern/logging.h"
#include "p2pipe/packet.h"
#include "p2pipe/pipe.h"
#include "p2pipe/version.h"
#include <ctype.h>
#include <stdint.h>
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

#define HANDSHAKE_LEN (sizeof(uint32_t))

size_t handshake_serialize(const Handshake* handshake, uint8_t* buffer, size_t len)
{
    if (!handshake|| !buffer) return 0;

    size_t total_size = HANDSHAKE_LEN;
    if (total_size > len) {
        return 0;
    }

    size_t offset = 0;

    memcpy(buffer + offset, &handshake->buffer_cap, sizeof(handshake->buffer_cap));
    offset += sizeof(handshake->buffer_cap);

    return offset;
}

bool handshake_deserialize(Handshake* handshake, const uint8_t* buffer, size_t len)
{
    if (!handshake || !buffer || len != HANDSHAKE_LEN) {
        return false;
    }

    size_t offset = 0;

    memcpy(&handshake->buffer_cap, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    return true;
}

static ssize_t udp_handshake_blocking(const char *server_ip,
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

static unsigned short get_available_udp_port(void)
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

int pipe_handshake(Pipe* pipe, const char* ip, size_t port, const char* id, const Handshake* handshake)
{
    char idkv[BASE56_LEN + 3 + 1];
    snprintf(idkv, sizeof(idkv), "ID=%s", id ? id : "N/A");
    char payload[512];
    snprintf(payload, sizeof(payload), "VERSION=%s TYPE=%s %s",
             VERSION_STRING,
             pipe->mode == MODE_SND ? "SND" : "RCV",
             (pipe->mode == MODE_RCV) ? idkv : "");

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

    if ((size_t)n >= sizeof(buf)) n = sizeof(buf) - 1;
    buf[n] = '\0';
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';

    if (strncmp(buf, "ID=", 3) == 0) {
        char assigned_id[BASE56_LEN + 1] = {0};
        size_t i = 0;
        const char *p = buf + 3;
        while (*p && !isspace((unsigned char)*p) && i < BASE56_LEN) {
            assigned_id[i++] = *p++;
        }
        assigned_id[i] = '\0';

        if (assigned_id[0] != '\0') {
            METRICS_SET(id, strdup(assigned_id));
            INFO("Server assigned session ID=%s; waiting for peer...", assigned_id);
        } else {
            ERRO("Server returned malformed ID reply: %s", buf);
            close(sock);
            return -1;
        }

        if (strstr(buf, "WAIT") != NULL) {
            for (;;) {
                struct sockaddr_storage from;
                socklen_t fromlen = sizeof(from);

                struct timeval tv = { .tv_sec = 360, .tv_usec = 0 };
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(sock, &rfds);

                int rv = select(sock + 1, &rfds, NULL, NULL, &tv);
                if (rv > 0 && FD_ISSET(sock, &rfds)) {
                    ssize_t r = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                         (struct sockaddr*)&from, &fromlen);
                    if (r <= 0) continue;
                    if ((size_t)r >= sizeof(buf)) r = sizeof(buf) - 1;
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
        } else if (strncmp(buf, "PEER ", 5) == 0) {
            // Unlikely. Fall-through
        } else {
            ERRO("Unknown server response after ID: %s", buf);
            close(sock);
            return -1;
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
             (parsed == 3 && peer_info[0]) ? peer_info : "(none)");

        char info_copy[512];
        info_copy[0] = '\0';
        if (parsed == 3) {
            strncpy(info_copy, peer_info, sizeof(info_copy) - 1);
            info_copy[sizeof(info_copy) - 1] = '\0';
        }

        char peer_version[128] = {0};
        char peer_id[BASE56_LEN + 1] = {0};

        if (parsed == 3 && info_copy[0]) {
            char *saveptr = NULL;
            char *token = strtok_r(info_copy, " ", &saveptr);
            while (token) {
                if (strncmp(token, "VERSION=", 8) == 0) {
                    strncpy(peer_version, token + 8, sizeof(peer_version) - 1);
                    peer_version[sizeof(peer_version) - 1] = '\0';
                    if (strcmp(peer_version, VERSION_STRING) != 0) {
                        WARN("Peer is using a different version: %s", peer_version);
                    }
                } else if (strncmp(token, "ID=", 3) == 0 && metrics.id == NULL) {
                    strncpy(peer_id, token + 3, sizeof(peer_id) - 1);
                    peer_id[sizeof(peer_id) - 1] = '\0';
                    metrics.id = strdup(peer_id);
                }
                token = strtok_r(NULL, " ", &saveptr);
            }
        }

        struct sockaddr_in peer_addr = {0};
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons((uint16_t)peer_port);
        if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
            ERRO("Invalid peer IP: %s", peer_ip);
            close(sock);
            return -1;
        }

        pipe->sock_fd = sock;
        pipe->peer_addr = peer_addr;

        Packet handshake_pkt;
        memset(&handshake_pkt, 0, sizeof(handshake_pkt));
        handshake_pkt.signals = SIGNAL_HANDSHAKE | SIGNAL_PAYLOAD;
        size_t len = handshake_serialize(handshake, handshake_pkt.data, sizeof(handshake_pkt.data));
        if (len == 0) {
            ERRO("Failed to serialize handshake info");
            close(sock);
            return -1;
        }
        handshake_pkt.len = len;

        if (!pipe_write_packet_sync(pipe, &handshake_pkt, NULL)) {
            ERRO("Failed to send handshake packet");
            close(sock);
            return -1;
        }

        return sock;
    }

    ERRO("Unknown handshake response: %s", buf);
    close(sock);
    return -1;
}

void handshake_print(const Handshake* handshake)
{
    printf("[HANDSHAKE PAYLOAD] buffer: %u\n", handshake->buffer_cap);
}
