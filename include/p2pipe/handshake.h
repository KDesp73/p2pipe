#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "p2pipe/pipe.h"
#include <stdio.h>

typedef struct {
    uint32_t buffer_cap;
    uint32_t payload_len;
    uint64_t hash;
} Handshake;
size_t handshake_serialize(const Handshake* handshake, uint8_t* buffer, size_t len);
bool handshake_deserialize(Handshake* handshake, const uint8_t* buffer, size_t len);
void handshake_print(const Handshake* handshake);

int pipe_handshake(Pipe* pipe, const char* ip, size_t port, const Handshake* handshake);


#endif // HANDSHAKE_H
