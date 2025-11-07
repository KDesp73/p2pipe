#ifndef STORAGE_H
#define STORAGE_H

#include "p2pipe/packet.h"
#include <stdint.h>
#include <stdio.h>
typedef struct {
    Packet** packets;
    size_t capacity;
    size_t count;
    bool ready;

    FILE* file_out;
    uint32_t next_expected_seq;
    bool stream_data;
} Storage;

bool storage_init(Storage* storage, size_t capacity, const char* dest_path, uint32_t initial_seq);
void storage_free(Storage* storage);
void storage_append(Storage* storage, const Packet* packet);
void storage_resize(Storage* storage, size_t capacity);
void storage_export(const Storage* storage);

#endif // STORAGE_H
