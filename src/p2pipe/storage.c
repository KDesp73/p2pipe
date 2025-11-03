#include "p2pipe/storage.h"
#include "extern/logging.h"
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
    storage->ready = true;;
    return true;
}

void storage_free(Storage* storage)
{
    if (!storage || !storage->ready)
        return;

    for (size_t i = 0; i < storage->count; ++i) {
        free(storage->packets[i]);
    }

    free(storage->packets);
    storage->packets = NULL;
    storage->count = 0;
    storage->capacity = 0;
    storage->ready = false;
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

void storage_append(Storage* storage, const Packet* packet)
{
    if (!storage || !packet)
        return;

    if (storage->count >= storage->capacity)
        storage_resize(storage, storage->capacity * 2);

    // Check if we already have this sequence number
    for (size_t i = 0; i < storage->count; ++i) {
        if (storage->packets[i] && storage->packets[i]->seq == packet->seq)
            return; // duplicate packet
    }

    Packet* copy = malloc(sizeof(Packet));
    if (!copy)
        return;
    memcpy(copy, packet, sizeof(Packet));

    size_t pos = storage->count;
    for (size_t i = 0; i < storage->count; ++i) {
        if (storage->packets[i]->seq > packet->seq) {
            pos = i;
            break;
        }
    }

    if (pos < storage->count) {
        memmove(&storage->packets[pos + 1],
                &storage->packets[pos],
                (storage->count - pos) * sizeof(Packet*));
    }

    storage->packets[pos] = copy;
    storage->count++;
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
        for (size_t j = i + 1; j < storage->count; ++j) {
            if (storage->packets[i]->seq > storage->packets[j]->seq) {
                Packet* tmp = storage->packets[i];
                storage->packets[i] = storage->packets[j];
                storage->packets[j] = tmp;
            }
        }
    }

    for (size_t i = 0; i < storage->count; ++i) {
        const Packet* pkt = storage->packets[i];
        if (pkt && pkt->len > 0)
            fwrite(pkt->data, 1, pkt->len, f);
    }

    fclose(f);
}

// TODO: If packet is received in-order append it immediately in the file and
// remove it from storage
