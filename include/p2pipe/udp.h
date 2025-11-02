#ifndef UDP_H
#define UDP_H

#include <stdio.h>

typedef struct {
    size_t buf; 
    size_t proto;
    char* id;
} HandshakeInfo;

int server(int port);

ssize_t udp_handshake_blocking(const char *server_ip,
        unsigned short server_port,
        const char *payload,
        char *out_buf,
        size_t out_buf_size,
        unsigned timeout_sec,
        unsigned max_retries,
        unsigned short client_port,
        int *out_sock);

unsigned short get_available_udp_port(void);

#endif // UDP_H
