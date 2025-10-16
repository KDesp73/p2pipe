#include "extern/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define BUFSIZE 1500

int sock;
struct sockaddr_in peer_addr;
int connected = 0;

static void *recv_thread(void *arg) {
    char buf[BUFSIZE];
    struct sockaddr_in from;
    socklen_t flen = sizeof(from);
    for (;;) {
        ssize_t n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&from, &flen);
        if (n <= 0) { usleep(100000); continue; }
        buf[n]=0;
        // if this is from the peer, mark connected
        if (from.sin_addr.s_addr == peer_addr.sin_addr.s_addr &&
            from.sin_port == peer_addr.sin_port) {
            if (!connected) {
                connected = 1;
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
                INFO("Received packet from peer %s:%d -> connection established", ip, ntohs(from.sin_port));
            }
            LOG(stderr, "PEER", "%s", buf);
        } else {
            // Other messages (e.g., server responses) are printed too
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
            fprintf(stderr, "[FROM %s:%d] %s\n", ip, ntohs(from.sin_port), buf);
        }
    }
    return NULL;
}

int client(char* server_ip, int server_port, char* myid)
{
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = 0; // 0 => let OS pick
    if (bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0) { perror("bind"); return 1; }

    // server address
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &server.sin_addr);
    server.sin_port = htons(server_port);

    // start receive thread
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    // Register with server
    char reg[128];
    snprintf(reg, sizeof(reg), "REGISTER %s\n", myid);
    if (sendto(sock, reg, strlen(reg), 0, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("sendto register");
        return 1;
    }
    INFO("Sent REGISTER to server %s:%d", server_ip, server_port);

    // Wait for server responses. We'll block read here for a while to get PEER instruction.
    char buf[BUFSIZE];
    struct sockaddr_in from;
    socklen_t flen = sizeof(from);

    // set a 15s overall deadline to receive peer info
    fd_set rfds;
    struct timeval tv;
    int got_peer = 0;
    for (int attempt=0; attempt<30 && !got_peer; ++attempt) {
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int s = select(sock+1, &rfds, NULL, NULL, &tv);
        if (s > 0 && FD_ISSET(sock, &rfds)) {
            ssize_t n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&from, &flen);
            if (n <= 0) continue;
            buf[n]=0;
            if (strncmp(buf, "PEER ", 5) == 0) {
                char ip[64];
                int port;
                char peerid[64];
                if (sscanf(buf+5, "%63s %d %63s", ip, &port, peerid) >= 2) {
                    INFO("Server gave peer %s:%d id=%s", ip, port, peerid);
                    memset(&peer_addr, 0, sizeof(peer_addr));
                    peer_addr.sin_family = AF_INET;
                    inet_pton(AF_INET, ip, &peer_addr.sin_addr);
                    peer_addr.sin_port = htons(port);
                    got_peer = 1;
                    break;
                }
            } else if (strncmp(buf, "NOPE", 4) == 0) {
                INFO("Server: NO PEER YET");
            } else {
                LOG(stderr, "SERVER", "%s\n", buf);
            }
        } // else timeout: keep waiting
    }

    if (!got_peer) {
        WARN("No peer info received. Exiting.");
        return 0;
    }

    // Begin hole punching: send several packets to peer while still listening.
    INFO("Starting hole punching: sending packets to peer...");
    for (int i = 0; i < 10; ++i) {
        const char *p = " punch ";
        sendto(sock, p, strlen(p), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        usleep(200000); // 200ms
    }
    INFO("Punch packets sent. If the peer does the same, connection may be established soon.");

    // Now interactive: read stdin lines and send to peer
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (connected) {
            sendto(sock, line, strlen(line), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        } else {
            // even if not "connected", still try
            sendto(sock, line, strlen(line), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
            INFO("Sent (not yet confirmed connection)");
        }
    }

    close(sock);
    return 0;
}

