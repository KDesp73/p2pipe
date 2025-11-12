#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t buffer_cap;
} Handshake;
size_t handshake_serialize(const Handshake* handshake, uint8_t* buffer, size_t len);
bool handshake_deserialize(Handshake* handshake, const uint8_t* buffer, size_t len);
void handshake_print(const Handshake* handshake);

#endif // HANDSHAKE_H
