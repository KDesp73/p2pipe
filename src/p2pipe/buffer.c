#include "p2pipe/buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief Initializes the Buffer structure by allocating memory for items.
 * @param buffer Pointer to the Buffer structure.
 * @param capacity The maximum number of packets the buffer can hold.
 * @return true on success, false on memory allocation failure.
 */
bool buffer_init(Buffer* buffer, size_t capacity) {
    if (!buffer) return false;
    
    buffer->items = (Packet*)calloc(capacity, sizeof(Packet));
    if (!buffer->items) {
        buffer->count = 0;
        buffer->capacity = 0;
        return false;
    }
    
    buffer->count = 0;
    buffer->capacity = capacity;
    return true;
}

/**
 * @brief Appends a Packet to the end of the buffer.
 * @param buffer Pointer to the Buffer structure.
 * @param packet The Packet to append (copied into the buffer).
 * @return true on success, false if the buffer is full.
 */
bool buffer_append(Buffer* buffer, Packet packet) {
    if (!buffer || buffer->count >= buffer->capacity) {
        return false; // Buffer is full
    }
    
    buffer->items[buffer->count] = packet;
    buffer->count++;
    
    return true;
}

/**
 * @brief Removes a Packet from the buffer identified by its sequence number.
 * * This uses a linear search and, upon removal, shifts subsequent elements 
 * to maintain a contiguous array starting at index 0.
 * * @param buffer Pointer to the Buffer structure.
 * @param seq The sequence number of the packet to remove.
 * @return true if a packet was found and removed, false otherwise.
 */
bool buffer_remove(Buffer* buffer, uint32_t seq) {
    if (!buffer || buffer->count == 0) return false;
    
    size_t i;
    for (i = 0; i < buffer->count; ++i) {
        if (buffer->items[i].seq == seq) {
            break;
        }
    }
    
    if (i == buffer->count) {
        return false;
    }
    
    size_t remaining_elements = buffer->count - 1 - i;
    if (remaining_elements > 0) {
        memmove(&buffer->items[i],
                &buffer->items[i + 1],
                remaining_elements * sizeof(Packet));
    }
    
    buffer->count--;
    
    return true;
}

/**
 * @brief Frees the heap-allocated memory associated with the buffer.
 * @param buffer Pointer to the Buffer structure.
 */
void buffer_free(Buffer* buffer) {
    if (!buffer) return;
    
    if (buffer->items) {
        free(buffer->items);
        buffer->items = NULL;
    }
    buffer->count = 0;
    buffer->capacity = 0;
}
