#ifndef PACKET_H
#define PACKET_H

#include <netinet/in.h>

typedef struct {
    void* data;
    size_t len;
    struct sockaddr_in peer;
} Packet;

#endif // PACKET_H
