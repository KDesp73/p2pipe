#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "p2pipe/udp.h"

#define BUFSIZE 1024

int server(int port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("bind"); return 1; }
    printf("[INFO] Bootstrap server listening on port %d\n", port);

    char buf[BUFSIZE];
    struct sockaddr_in stored_peer;
    char stored_info[BUFSIZE];
    int have_peer = 0;

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&cli, &clen);
        if (n <= 0) continue;
        buf[n] = 0;

        // Expect a handshake message
        if (strncmp(buf, "HANDSHAKE ", 10) == 0) {
            char *info = buf + 10;

            char cli_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
            int cli_port = ntohs(cli.sin_port);
            printf("[INFO] HANDSHAKE from %s:%d (%s)\n", cli_ip, cli_port, info);

            if (!have_peer) {
                stored_peer = cli;
                strncpy(stored_info, info, sizeof(stored_info) - 1);
                have_peer = 1;
                const char *reply = "WAIT\n";
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr*)&cli, clen);
                printf(" -> replied WAIT (waiting for another peer)\n");
            } else {
                char stored_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &stored_peer.sin_addr, stored_ip, sizeof(stored_ip));
                int stored_port = ntohs(stored_peer.sin_port);

                char reply[BUFSIZE];
                snprintf(reply, sizeof(reply),
                         "PEER %s %d %.900s\n",
                         stored_ip, stored_port, stored_info);
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr*)&cli, clen);
                printf(" -> sent PEER %s:%d to new client\n", stored_ip, stored_port);

                char new_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cli.sin_addr, new_ip, sizeof(new_ip));
                int new_port = ntohs(cli.sin_port);
                char reply2[BUFSIZE];
                snprintf(reply2, sizeof(reply2),
                         "PEER %s %d %.900s\n",
                         new_ip, new_port, info);
                sendto(sock, reply2, strlen(reply2), 0, (struct sockaddr*)&stored_peer, sizeof(stored_peer));
                printf(" -> sent PEER %s:%d to stored peer\n", new_ip, new_port);

                have_peer = 0;
                stored_info[0] = 0;
            }
        } else {
            const char *r = "ERR unknown\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
        }
    }

    close(sock);
    return 0;
}
