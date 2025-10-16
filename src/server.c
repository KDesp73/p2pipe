#include "extern/logging.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFSIZE 512

int server(int port) 
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("bind"); return 1; }
    INFO("Bootstrap server listening on port %d", port);

    char buf[BUFSIZE];
    // store one peer (very simple)
    struct sockaddr_in stored_peer;
    int have_peer = 0;
    char stored_id[64] = {0};

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&cli, &clen);
        if (n <= 0) continue;
        buf[n] = 0;

        // parse simple REGISTER command
        if (strncmp(buf, "REGISTER ", 9) == 0) {
            char id[64]; id[0]=0;
            sscanf(buf+9, "%63s", id);
            char cli_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
            int cli_port = ntohs(cli.sin_port);
            INFO("REGISTER from %s:%d id=%s", cli_ip, cli_port, id);

            if (!have_peer) {
                // store and reply NOPE (no peer yet)
                stored_peer = cli;
                strncpy(stored_id, id, sizeof(stored_id)-1);
                have_peer = 1;
                const char *reply = "NOPE\n";
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr*)&cli, clen);
                printf(" -> replied NOPE (waiting for another peer)\n");
            } else {
                // if stored peer is same as this client, just say NOPE
                if (stored_peer.sin_addr.s_addr == cli.sin_addr.s_addr &&
                    stored_peer.sin_port == cli.sin_port) {
                    sendto(sock, "NOPE\n", 5, 0, (struct sockaddr*)&cli, clen);
                    printf(" -> same peer re-registered, replied NOPE\n");
                } else {
                    // send the stored peer info to this client
                    char stored_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &stored_peer.sin_addr, stored_ip, sizeof(stored_ip));
                    int stored_port = ntohs(stored_peer.sin_port);

                    char reply[128];
                    snprintf(reply, sizeof(reply), "PEER %s %d %s\n", stored_ip, stored_port, stored_id);
                    sendto(sock, reply, strlen(reply), 0, (struct sockaddr*)&cli, clen);
                    printf(" -> sent PEER %s:%d to new client\n", stored_ip, stored_port);

                    // also send this new client's info back to stored peer so stored peer can punch
                    char new_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cli.sin_addr, new_ip, sizeof(new_ip));
                    int new_port = ntohs(cli.sin_port);
                    char reply2[128];
                    snprintf(reply2, sizeof(reply2), "PEER %s %d %s\n", new_ip, new_port, id);
                    sendto(sock, reply2, strlen(reply2), 0, (struct sockaddr*)&stored_peer, sizeof(stored_peer));
                    printf(" -> sent PEER %s:%d to stored peer\n", new_ip, new_port);

                    // clear stored_peer so next REGISTER will start fresh
                    have_peer = 0;
                    stored_id[0] = 0;
                }
            }
        } else {
            // unknown command
            const char *r = "ERR unknown\n";
            sendto(sock, r, strlen(r), 0, (struct sockaddr*)&cli, clen);
        }
    }

    close(sock);
    return 0;
}

