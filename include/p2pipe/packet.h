#ifndef PACKET_H
#define PACKET_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    SIGNAL_PAYLOAD = 1 << 0,
    SIGNAL_ACK     = 1 << 1,
    SIGNAL_RESEND  = 1 << 2,
    SIGNAL_END     = 1 << 3
} PacketSignal;

#define PACKET_BUFFER_SIZE 1024

typedef struct {
    uint8_t signals;
    uint32_t seq;
    uint32_t len;
    uint8_t data[PACKET_BUFFER_SIZE];
} Packet;

#define PACKET_END (Packet) { .signals = SIGNAL_END, .len = 0 }
#define PACKET_ACK(n) (Packet) { .signals = SIGNAL_ACK, .seq = n, .len = 0 }
#define PACKET_RESEND(n) (Packet) { .signals = SIGNAL_RESEND, .seq = n, .len = 0 }

size_t packet_serialize(const Packet* packet, uint8_t* buffer, size_t buffer_len);
bool packet_deserialize(Packet* packet, const uint8_t* buffer, size_t buf_len);


#endif // PACKET_H
