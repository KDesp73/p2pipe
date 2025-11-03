#ifndef BUFFER_H
#define BUFFER_H


#include "p2pipe/packet.h"
#include <stdint.h>

typedef struct {
    Packet* items;
    size_t count;
    size_t capacity;
} Buffer;

bool buffer_init(Buffer* buffer, size_t capacity);
bool buffer_append(Buffer* buffer, Packet packet);
bool buffer_remove(Buffer* buffer, uint32_t seq);
void buffer_free(Buffer* buffer);

#endif // BUFFER_H
