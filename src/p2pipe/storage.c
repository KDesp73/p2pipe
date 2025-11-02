#include "p2pipe/storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool storage_init(Storage* storage, size_t capacity)
{
    if (!storage || capacity == 0)
        return false;

    storage->packets = calloc(capacity, sizeof(Packet*));
    if (!storage->packets)
        return false;

    storage->count = 0;
    storage->capacity = capacity;
    return true;
}

void storage_free(Storage* storage)
{
    if (!storage)
        return;

    for (size_t i = 0; i < storage->count; ++i) {
        free(storage->packets[i]);
    }

    free(storage->packets);
    storage->packets = NULL;
    storage->count = 0;
    storage->capacity = 0;
}

void storage_append(Storage* storage, const Packet* packet)
{
    if (!storage || !packet)
        return;

    if (storage->count >= storage->capacity) {
        size_t new_capacity = storage->capacity * 2;
        storage_resize(storage, new_capacity);
    }

    Packet* copy = malloc(sizeof(Packet));
    if (!copy)
        return;

    memcpy(copy, packet, sizeof(Packet));
    storage->packets[storage->count++] = copy;
}

void storage_resize(Storage* storage, size_t capacity)
{
    if (!storage || capacity == 0)
        return;

    Packet** new_packets = realloc(storage->packets, capacity * sizeof(Packet*));
    if (!new_packets)
        return;

    if (capacity > storage->capacity) {
        memset(new_packets + storage->capacity, 0,
               (capacity - storage->capacity) * sizeof(Packet*));
    }

    storage->packets = new_packets;
    storage->capacity = capacity;
}

void storage_export(const Storage* storage, const char* path)
{
    if (!storage || !path)
        return;

    FILE* f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return;
    }

    for (size_t i = 0; i < storage->count; ++i) {
        const Packet* pkt = storage->packets[i];
        if (pkt && pkt->len > 0) {
            fwrite(pkt->data, 1, pkt->len, f);
        }
    }

    fclose(f);
}
