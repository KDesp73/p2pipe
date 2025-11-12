#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "p2pipe/id.h"

#define BUFSIZE 2048
#define MAX_SESSIONS 1024

typedef struct {
    char id[BASE56_LEN + 1];
    struct sockaddr_in sender_addr;
    struct sockaddr_in receiver_addr;
    char sender_info[BUFSIZE];
    char receiver_info[BUFSIZE];
    int have_sender;
    int have_receiver;
    time_t last_activity;
} session_t;

static session_t sessions[MAX_SESSIONS];

static session_t *find_session_by_id(const char *id) {
    if (!id || !*id) return NULL;
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].id[0] != '\0' && strcmp(sessions[i].id, id) == 0) {
            return &sessions[i];
        }
    }
    return NULL;
}

static session_t *create_session(void) {
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].id[0] == '\0' && !sessions[i].have_sender && !sessions[i].have_receiver) {
            memset(&sessions[i], 0, sizeof(session_t));
            sessions[i].last_activity = time(NULL);
            return &sessions[i];
        }
    }
    return NULL;
}

static void remove_session(session_t *s) {
    if (!s) return;
    memset(s, 0, sizeof(session_t));
}

static void extract_token_value(const char *buf, const char *key, char *out, size_t outlen) {
    // Finds key=VALUE in buf and copies VALUE into out
    out[0] = '\0';
    const char *p = strstr(buf, key);
    if (!p) return;
    p += strlen(key);
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '\n' && i + 1 < outlen) {
        out[i++] = *p++;
    }
    out[i] = '\0';
}

int server(int port) {
    srand((unsigned)time(NULL));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("[INFO] Bootstrap server listening on port %d\n", port);

    char buf[BUFSIZE];

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&cli, &clen);
        if (n <= 0) {
            if (n < 0) perror("recvfrom");
            continue;
        }
        buf[n] = '\0';

        if (strncmp(buf, "HANDSHAKE ", 10) != 0) {
            const char *r = "ERR unknown\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
            continue;
        }

        char type[8] = {0};
        if (sscanf(buf + 10, "%*s TYPE=%7s", type) != 1) {
            extract_token_value(buf + 10, "TYPE=", type, sizeof(type));
            if (type[0] == '\0') {
                const char *r = "ERR malformed TYPE\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                continue;
            }
        }

        for (char *p = type; *p; ++p) *p = (char)toupper((unsigned char)*p);

        char cli_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
        int cli_port = ntohs(cli.sin_port);
        printf("[INFO] HANDSHAKE from %s:%d (%s)\n", cli_ip, cli_port, buf + 10);

        if (strcmp(type, "SND") == 0) {
            session_t *s = create_session();
            if (!s) {
                const char *r = "ERR no capacity\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                continue;
            }

            s->sender_addr = cli;
            s->have_sender = 1;
            s->last_activity = time(NULL);
            strncpy(s->sender_info, buf + 10, sizeof(s->sender_info) - 1);

            generate_base56_id(s->id, BASE56_LEN);
            s->id[BASE56_LEN] = '\0';

            char reply[BUFSIZE];
            snprintf(reply, sizeof(reply), "ID=%.16s WAIT\n", s->id);
            sendto(sock, reply, strlen(reply), 0, (struct sockaddr*)&cli, clen);

            printf(" -> created session ID=%s for sender %s:%d\n", s->id, cli_ip, cli_port);
            continue;
        } else if (strcmp(type, "RCV") == 0) {
            // Receiver must provide ID=...
            char id_val[BASE56_LEN + 1] = {0};
            extract_token_value(buf + 10, "ID=", id_val, sizeof(id_val));
            if (id_val[0] == '\0') {
                const char *r = "ERR missing ID\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                continue;
            }

            session_t *s = find_session_by_id(id_val);
            if (!s) {
                const char *r = "ERR no such session\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                printf(" -> receiver %s:%d attempted unknown ID=%s\n", cli_ip, cli_port, id_val);
                continue;
            }

            if (!s->have_sender) {
                const char *r = "ERR sender not present\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                printf(" -> receiver %s:%d tried to join ID=%s but sender missing\n", cli_ip, cli_port, id_val);
                continue;
            }

            if (s->have_receiver) {
                const char *r = "ERR receiver already connected\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                printf(" -> duplicate receiver for ID=%s from %s:%d\n", id_val, cli_ip, cli_port);
                continue;
            }

            // attach receiver
            s->receiver_addr = cli;
            s->have_receiver = 1;
            s->last_activity = time(NULL);
            strncpy(s->receiver_info, buf + 10, sizeof(s->receiver_info) - 1);

            // send receiver info to sender
            char snd_ip[INET_ADDRSTRLEN], rcv_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &s->sender_addr.sin_addr, snd_ip, sizeof(snd_ip));
            inet_ntop(AF_INET, &s->receiver_addr.sin_addr, rcv_ip, sizeof(rcv_ip));
            int snd_port = ntohs(s->sender_addr.sin_port);
            int rcv_port = ntohs(s->receiver_addr.sin_port);

            char reply_to_snd[BUFSIZE];
            snprintf(reply_to_snd, sizeof(reply_to_snd),
                     "PEER %s %d %.990s ID=%.16s\n", rcv_ip, rcv_port, s->receiver_info, s->id);
            sendto(sock, reply_to_snd, strlen(reply_to_snd), 0,
                   (struct sockaddr*)&s->sender_addr, sizeof(s->sender_addr));
            printf(" -> sent receiver info to sender (%s:%d) with ID %s\n", rcv_ip, rcv_port, s->id);

            char reply_to_rcv[BUFSIZE];
            snprintf(reply_to_rcv, sizeof(reply_to_rcv),
                     "PEER %s %d %.990s ID=%.16s\n", snd_ip, snd_port, s->sender_info, s->id);
            sendto(sock, reply_to_rcv, strlen(reply_to_rcv), 0,
                   (struct sockaddr*)&s->receiver_addr, sizeof(s->receiver_addr));
            printf(" -> sent sender info to receiver (%s:%d) with ID %s\n", snd_ip, snd_port, s->id);

            // matched, remove session so ID isn't reused immediately
            remove_session(s);
            continue;
        } else {
            const char *r = "ERR invalid TYPE\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
            continue;
        }
    }

    close(sock);
    return 0;
}
