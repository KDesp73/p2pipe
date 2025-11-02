#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "p2pipe/id.h"
#include "p2pipe/udp.h"

#define BUFSIZE 2048

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
        return 1;
    }

    printf("[INFO] Bootstrap server listening on port %d\n", port);

    char buf[BUFSIZE];
    struct sockaddr_in sender_addr = {0}, receiver_addr = {0};
    char sender_info[BUFSIZE] = {0}, receiver_info[BUFSIZE] = {0};
    int have_sender = 0, have_receiver = 0;
    char pair_id[BASE56_LEN + 1] = {0};

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&cli, &clen);
        if (n <= 0) continue;
        buf[n] = 0;

        if (strncmp(buf, "HANDSHAKE ", 10) != 0) {
            const char *r = "ERR unknown\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
            continue;
        }

        char *info = buf + 10;
        char type[8] = {0};
        if (sscanf(info, "%*s %*s TYPE=%7s", type) != 1) {
            const char *r = "ERR malformed TYPE\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
            continue;
        }

        char cli_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
        int cli_port = ntohs(cli.sin_port);
        printf("[INFO] HANDSHAKE from %s:%d (%s)\n", cli_ip, cli_port, info);

        if (strcmp(type, "SND") == 0) {
            if (have_sender) {
                const char *r = "ERR sender already connected\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                continue;
            }
            sender_addr = cli;
            strncpy(sender_info, info, sizeof(sender_info) - 1);
            have_sender = 1;
        } else if (strcmp(type, "RCV") == 0) {
            if (have_receiver) {
                const char *r = "ERR receiver already connected\n";
                sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
                continue;
            }
            receiver_addr = cli;
            strncpy(receiver_info, info, sizeof(receiver_info) - 1);
            have_receiver = 1;
        } else {
            const char *r = "ERR invalid TYPE\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
            continue;
        }

        if (!(have_sender && have_receiver)) {
            const char *wait_reply = "WAIT\n";
            sendto(sock, wait_reply, strlen(wait_reply), 0, (struct sockaddr*)&cli, clen);
            printf(" -> replied WAIT (waiting for counterpart)\n");
            continue;
        }

        generate_base56_id(pair_id, BASE56_LEN);

        char snd_ip[INET_ADDRSTRLEN], rcv_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, snd_ip, sizeof(snd_ip));
        inet_ntop(AF_INET, &receiver_addr.sin_addr, rcv_ip, sizeof(rcv_ip));
        int snd_port = ntohs(sender_addr.sin_port);
        int rcv_port = ntohs(receiver_addr.sin_port);

        char reply_to_snd[BUFSIZE];
        snprintf(reply_to_snd, sizeof(reply_to_snd),
                 "PEER %s %d %.990s ID=%.16s\n", rcv_ip, rcv_port, receiver_info, pair_id);
        sendto(sock, reply_to_snd, strlen(reply_to_snd), 0,
               (struct sockaddr*)&sender_addr, sizeof(sender_addr));
        printf(" -> sent receiver info to sender (%s:%d) with ID %s\n", rcv_ip, rcv_port, pair_id);

        char reply_to_rcv[BUFSIZE];
        snprintf(reply_to_rcv, sizeof(reply_to_rcv),
                 "PEER %s %d %.990s ID=%.16s\n", snd_ip, snd_port, sender_info, pair_id);
        sendto(sock, reply_to_rcv, strlen(reply_to_rcv), 0,
               (struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
        printf(" -> sent sender info to receiver (%s:%d) with ID %s\n", snd_ip, snd_port, pair_id);

        // Reset for next pair
        have_sender = 0;
        have_receiver = 0;
        memset(sender_info, 0, sizeof(sender_info));
        memset(receiver_info, 0, sizeof(receiver_info));
        pair_id[0] = 0;
    }

    close(sock);
    return 0;
}
