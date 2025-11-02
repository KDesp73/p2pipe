#include "p2pipe/packet.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


size_t packet_serialize(const Packet* packet, uint8_t* buffer, size_t buffer_len)
{
    if (!packet || !buffer) return 0;

    size_t total_size = 1 + 4 + 4 + packet->len;
    if (total_size > buffer_len) {
        return 0;
    }

    size_t offset = 0;

    buffer[offset++] = packet->signals;

    uint32_t seq_n = htonl(packet->seq);
    memcpy(buffer + offset, &seq_n, sizeof(seq_n));
    offset += sizeof(seq_n);

    uint32_t len_n = htonl(packet->len);
    memcpy(buffer + offset, &len_n, sizeof(len_n));
    offset += sizeof(len_n);

    memcpy(buffer + offset, packet->data, packet->len);
    offset += packet->len;

    return offset;
}

bool packet_deserialize(Packet* packet, const uint8_t* buffer, size_t buf_len)
{
    if (!packet || !buffer || buf_len < sizeof(uint8_t) + 2 * sizeof(uint32_t)) {
        return false;
    }

    size_t offset = 0;

    // signals (1 byte)
    packet->signals = buffer[offset++];
    
    // seq (4 bytes)
    uint32_t seq_net = 0;
    memcpy(&seq_net, buffer + offset, sizeof(uint32_t));
    packet->seq = ntohl(seq_net);
    offset += sizeof(uint32_t);

    // len (4 bytes)
    uint32_t len_net = 0;
    memcpy(&len_net, buffer + offset, sizeof(uint32_t));
    packet->len = ntohl(len_net);
    offset += sizeof(uint32_t);

    if (packet->len > PACKET_BUFFER_SIZE) {
        packet->len = PACKET_BUFFER_SIZE;
    }

    if (offset + packet->len > buf_len) {
        packet->len = 0;
        return false;
    }

    memcpy(packet->data, buffer + offset, packet->len);

    return true;
}
