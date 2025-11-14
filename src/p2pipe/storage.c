#include "p2pipe/storage.h"
#include "extern/logging.h"
#include "p2pipe/log.h"
#include "p2pipe/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Deliver any in-order packets.
 * With Packet* storage (full structs), nothing needs to be freed individually.
 */
static void storage_try_deliver(Storage* storage)
{
    if (!storage) return;

    pthread_mutex_lock(&storage->lock);

    while (storage->count > 0 &&
           storage->packets[0].seq == storage->next_expected_seq)
    {
        Packet* pkt = &storage->packets[0];

        if (storage->file_out && pkt->len > 0) {
            size_t written = fwrite(pkt->data, 1, pkt->len, storage->file_out);
            if (written != pkt->len) {
                ERRO("Partial write at seq #%u", pkt->seq);
                break;
            }
        }

        storage->next_expected_seq++;

        /* shift array */
        if (storage->count > 1)
            memmove(&storage->packets[0],
                    &storage->packets[1],
                    (storage->count - 1) * sizeof(Packet));

        storage->count--;
    }

    pthread_mutex_unlock(&storage->lock);
}


/**
 * Initialize storage with Packet* array (array of full structs).
 */
bool storage_init(Storage* storage, size_t capacity,
                  const char* dest_path, uint32_t initial_seq)
{
    if (!storage || capacity == 0 || !dest_path)
        return false;

    storage->packets = calloc(capacity, sizeof(Packet));
    if (!storage->packets)
        return false;

    storage->file_out = fopen(dest_path, "wb");
    if (!storage->file_out) {
        ERRO("Failed to open destination file: %s", dest_path);
        free(storage->packets);
        return false;
    }

    storage->count = 0;
    storage->capacity = capacity;
    storage->next_expected_seq = initial_seq;
    storage->ready = true;
    storage->stream_data = false;

    if (pthread_mutex_init(&storage->lock, NULL) != 0) {
        ERRO("Failed to init mutex");
        fclose(storage->file_out);
        free(storage->packets);
        return false;
    }

    return true;
}


/**
 * Free all resources. No per-packet frees required.
 */
void storage_free(Storage* storage)
{
    if (!storage || !storage->ready)
        return;

    pthread_mutex_lock(&storage->lock);

    if (storage->file_out) {
        fclose(storage->file_out);
        storage->file_out = NULL;
    }

    free(storage->packets);
    storage->packets = NULL;

    storage->count = 0;
    storage->capacity = 0;
    storage->ready = false;

    pthread_mutex_unlock(&storage->lock);
    pthread_mutex_destroy(&storage->lock);
}


/**
 * Resize the array of Packet structs.
 */
void storage_resize(Storage* storage, size_t capacity)
{
    if (!storage || capacity == 0)
        return;

    pthread_mutex_lock(&storage->lock);

    Packet* new_packets = realloc(storage->packets, capacity * sizeof(Packet));
    if (!new_packets) {
        WARN("storage_resize: realloc failed");
        pthread_mutex_unlock(&storage->lock);
        return;
    }

    if (capacity > storage->capacity) {
        memset(new_packets + storage->capacity, 0,
               (capacity - storage->capacity) * sizeof(Packet));
    }

    storage->packets = new_packets;
    storage->capacity = capacity;

    pthread_mutex_unlock(&storage->lock);
}


/**
 * Append a packet (copy the struct), keep array sorted, then try deliver.
 */
void storage_append(Storage* storage, const Packet* packet)
{
    if (!storage || !packet)
        return;

    pthread_mutex_lock(&storage->lock);

    /* Grow array if needed */
    if (storage->count >= storage->capacity) {
        size_t newcap = storage->capacity ? storage->capacity * 2 : 8;

        Packet* newbuf = realloc(storage->packets, newcap * sizeof(Packet));
        if (!newbuf) {
            WARN("storage_append: resize failed, dropping seq #%u", packet->seq);
            METRICS_INCR(packets_discarded);
            pthread_mutex_unlock(&storage->lock);
            return;
        }

        storage->packets = newbuf;
        storage->capacity = newcap;
    }

    /* Insert while keeping sequence order */
    size_t pos = storage->count;
    while (pos > 0 && storage->packets[pos - 1].seq > packet->seq) {
        storage->packets[pos] = storage->packets[pos - 1];
        pos--;
    }

    storage->packets[pos] = *packet;
    storage->count++;

    pthread_mutex_unlock(&storage->lock);

    /* Attempt delivery (lock inside) */
    storage_try_deliver(storage);
}


/**
 * Write any remaining packets (out-of-order leftovers).
 */
void storage_export(const Storage* storage)
{
    if (!storage || !storage->file_out)
        return;

    /* bubble sort — bad but unchanged behavior */
    for (size_t i = 0; i < storage->count; ++i) {
        for (size_t j = i + 1; j < storage->count; ++j) {
            if (storage->packets[i].seq > storage->packets[j].seq) {
                Packet tmp = storage->packets[i];
                ((Packet*)storage->packets)[i] = storage->packets[j];
                ((Packet*)storage->packets)[j] = tmp;
            }
        }
    }

    /* Write packets */
    for (size_t i = 0; i < storage->count; ++i) {
        const Packet* pkt = &storage->packets[i];
        if (pkt->len > 0)
            fwrite(pkt->data, 1, pkt->len, storage->file_out);
    }
}
