#ifndef STORAGE_H
#define STORAGE_H

#include "p2pipe/packet.h"
#include <stdint.h>
typedef struct {
    Packet** packets;
    size_t capacity;
    size_t count;
    bool ready;
} Storage;

bool storage_init(Storage* storage, size_t capacity);
void storage_free(Storage* storage);
void storage_append(Storage* storage, const Packet* packet);
void storage_resize(Storage* storage, size_t capacity);
void storage_export(const Storage* storage, const char* path);


#endif // STORAGE_H
