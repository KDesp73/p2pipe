#include "p2pipe/storage.h"
#include "extern/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Attempts to deliver any contiguous, in-order packets to the destination file.
 * * This function iterates through the stored packets (which are sorted by seq number). 
 * If the first packet's sequence number matches the next expected sequence number,
 * it is written to the file, freed, and removed from the storage array.
 * This process continues until an out-of-order packet is encountered.
 * * @param storage Pointer to the Storage structure.
 */
static void storage_try_deliver(Storage* storage)
{
    // Write and remove packets as long as the next expected packet is at the head of the array
    while (storage->count > 0 && storage->packets[0]->seq == storage->next_expected_seq) {
        Packet* pkt = storage->packets[0];

        // 1. Write the packet data to the file
        if (storage->file_out && pkt->len > 0) {
            size_t written = fwrite(pkt->data, 1, pkt->len, storage->file_out);
            if (written != pkt->len) {
                ERRO("Partial write to file during delivery of seq #%u. Aborting delivery.", pkt->seq);
                // Stop delivery on a write failure
                break; 
            }
        }
        
        // 2. Advance the expected sequence number
        storage->next_expected_seq++; 

        // 3. Remove the packet from storage and free its memory
        free(pkt);

        // Shift remaining packets up to fill the gap (O(N) operation)
        if (storage->count > 1) {
            memmove(&storage->packets[0], &storage->packets[1], (storage->count - 1) * sizeof(Packet*));
        }
        
        // Decrement count and clear the now-empty last pointer
        storage->count--;
        if (storage->count < storage->capacity) {
            storage->packets[storage->count] = NULL;
        }
    }
}

/**
 * @brief Initializes the storage buffer and opens the destination file.
 * * @param storage Pointer to the Storage structure.
 * @param capacity The initial capacity of the packet array.
 * @param dest_path The path to the destination file.
 * @param initial_seq The sequence number of the first expected packet (e.g., 1 or 0).
 * @return true on success, false on failure.
 */
bool storage_init(Storage* storage, size_t capacity, const char* dest_path, uint32_t initial_seq)
{
    if (!storage || capacity == 0 || !dest_path)
        return false;

    storage->packets = calloc(capacity, sizeof(Packet*));
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
    storage->next_expected_seq = initial_seq; // Initialize expected sequence
    storage->ready = true;
    return true;
}

/**
 * @brief Frees all allocated memory and closes the destination file.
 * * @param storage Pointer to the Storage structure.
 */
void storage_free(Storage* storage)
{
    if (!storage || !storage->ready)
        return;

    // Close the file output handle
    if (storage->file_out) {
        fclose(storage->file_out);
        storage->file_out = NULL;
    }

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
        // Zero out the newly allocated space
        memset(new_packets + storage->capacity, 0,
               (capacity - storage->capacity) * sizeof(Packet*));
    }

    storage->packets = new_packets;
    storage->capacity = capacity;
}

/**
 * @brief Appends a new packet to the storage, keeping the array sorted by sequence number,
 * and then attempts to deliver contiguous data.
 * * @param storage Pointer to the Storage structure.
 * @param packet The packet to be stored.
 */
void storage_append(Storage* storage, const Packet* packet)
{
    if (!storage || !packet)
        return;

    if (storage->count >= storage->capacity)
        storage_resize(storage, storage->capacity * 2);

    // 1. Check for duplicate or already delivered packet (delivery is prioritized by seq)
    if (packet->seq < storage->next_expected_seq) {
        INFO("Dropping duplicate/already delivered packet #%u", packet->seq);
        return;
    }
    
    for (size_t i = 0; i < storage->count; ++i) {
        if (storage->packets[i] && storage->packets[i]->seq == packet->seq) {
            INFO("Dropping duplicate packet #%u found in storage", packet->seq);
            return; // duplicate packet in buffer
        }
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

    if(storage->stream_data)
        storage_try_deliver(storage);
}

/**
 * @brief Writes any remaining packets in storage to the destination file.
 * NOTE: This function is mostly obsolete in the new design where data is streamed
 * but is kept to write any out-of-order data remaining on shutdown/export request.
 * * @param storage Pointer to the Storage structure.
 * @param path The path to the file (unused, as file is already open).
 */
void storage_export(const Storage* storage, const char* path)
{
    if (!storage || !storage->file_out)
        return;

    // NOTE: This sorting should ideally be removed as it's slow.
    for (size_t i = 0; i < storage->count; ++i) {
        for (size_t j = i + 1; j < storage->count; ++j) {
            if (storage->packets[i]->seq > storage->packets[j]->seq) {
                Packet* tmp = storage->packets[i];
                // Cast to non-const pointer to allow swap
                ((Packet**)storage->packets)[i] = storage->packets[j]; 
                ((Packet**)storage->packets)[j] = tmp;
            }
        }
    }

    for (size_t i = 0; i < storage->count; ++i) {
        const Packet* pkt = storage->packets[i];
        if (pkt && pkt->len > 0)
            fwrite(pkt->data, 1, pkt->len, storage->file_out);
    }
}
