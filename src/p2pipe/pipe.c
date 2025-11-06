#include "p2pipe/pipe.h"
#include "extern/logging.h"
#include "p2pipe/buffer.h"
#include "p2pipe/handshake.h"
#include "p2pipe/packet.h"
#include "p2pipe/storage.h"
#include "p2pipe/helpers.h"
#include "p2pipe/threads.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TIMES(n) for(size_t i = 0; i < (n); i++)

ThreadPool* tp = NULL;
OrderedExecutor* oe = NULL;

int pipe_rcv_open(Pipe* pipe, const char* ip, size_t port, size_t capacity)
{
    if (!pipe || !ip) return -1;

    pipe_init(pipe, capacity);
    pipe->mode = MODE_RCV;
    pipe->running = true;

    TIMES(3) thread_pool_submit(tp, packet_listener, pipe);

    Handshake handshake = {
        .buffer_cap = capacity,
        .payload_len = 0,
        .hash = 0
    };
    int sock = pipe_handshake(pipe, ip, port, &handshake);
    if (sock < 0) {
        pipe_free(pipe);
        return -1;
    }

    return sock;
}

bool pipe_read(Pipe* pipe, size_t n_bytes)
{
    if (!pipe || pipe->mode != MODE_RCV || pipe->sock_fd < 0) {
        return false;
    }
    if (n_bytes == 0) return true;

    pthread_mutex_lock(&pipe->storage_lock);

    while (pipe->storage.count == 0 && pipe->running) {
        INFO("Pipe read blocking: Waiting for data...");
        pthread_cond_wait(&pipe->storage_cond, &pipe->storage_lock);
    }
    
    if (!pipe->running && pipe->storage.count == 0) {
        INFO("Pipe closed during read.");
        pthread_mutex_unlock(&pipe->storage_lock);
        return false; 
    }

    pthread_mutex_unlock(&pipe->storage_lock);
    
    return true;
}


void pipe_rcv_close(Pipe* pipe)
{
    if (!pipe) return;
    
    while(!pipe->end_received) {
        usleep(1000);
    }
    
    pipe->running = false;
    pthread_cond_broadcast(&pipe->storage_cond);

    if (pipe->sock_fd >= 0) {
        close(pipe->sock_fd); 
        pipe->sock_fd = -1;
    }

    thread_pool_wait(tp);
    pipe_free(pipe);
    INFO("Pipe receiver closed");
}

int pipe_snd_open(Pipe* pipe, const char* ip, size_t port, size_t capacity, void* payload, size_t len)
{
    pipe_init(pipe, capacity);
    pipe->mode = MODE_SND;
    pipe->running = true;

    TIMES(3) thread_pool_submit(tp, packet_listener, pipe);

    Handshake handshake = {
        .buffer_cap = capacity,
        .payload_len = len,
        .hash = compute_fnv1a_hash(payload, len)
    };
    int sock = pipe_handshake(pipe, ip, port, &handshake);
    if (sock < 0) {
        pipe_free(pipe);
        return -1;
    }

    TIMES(3) thread_pool_submit(tp, retransmission_thread, pipe);

    return sock;
}

bool pipe_write(Pipe* pipe, void* payload, size_t len)
{
    if (!pipe || pipe->mode != MODE_SND || pipe->sock_fd < 0)
        return false;

    uint8_t* data = payload;
    size_t offset = 0;
    
    uint32_t seq = pipe->seq; 

    while (offset < len) {
        if(!pipe->handshake_completed) continue;
        Packet packet = {0};
        packet.signals = SIGNAL_PAYLOAD;
        packet.seq = seq++;
        packet.len = (len - offset) < PACKET_BUFFER_SIZE ? (len - offset) : PACKET_BUFFER_SIZE;
        packet.last_sent_ms = current_time_ms();
        memcpy(packet.data, data + offset, packet.len);

        pthread_mutex_lock(&pipe->ack_lock);

        while (pipe->buffer.count >= pipe->buffer.capacity) {
            WARN("Buffer full (%zu/%zu). Waiting for ACKs...", pipe->buffer.count, pipe->buffer.capacity);
            pthread_cond_wait(&pipe->ack_cond, &pipe->ack_lock);
        }

        if (!buffer_append(&pipe->buffer, packet)) {
            ERRO("Failed to append packet to buffer");
            pthread_mutex_unlock(&pipe->ack_lock);
            return false;
        }

        pthread_mutex_unlock(&pipe->ack_lock); 

        if(!pipe_write_packet_async(pipe, &packet, NULL)) {
           ERRO("Could not submit packet for sending");
           continue;
        }

        offset += packet.len;
    }

    pipe->seq = seq; 
    return true;
}

bool pipe_flush(Pipe* pipe)
{
    if (!pipe || pipe->mode != MODE_SND || pipe->sock_fd <= 0) {
        return false;
    }

    pthread_mutex_lock(&pipe->ack_lock);
    
    if (pipe->buffer.count == 0) {
        pthread_mutex_unlock(&pipe->ack_lock);
        INFO("Pipe is already flushed.");
        return true;
    }

    struct timespec ts;
    bool success = true;

    while (pipe->buffer.count > 0) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5; 

        INFO("Flushing pipe: Waiting for %zu packets to be acknowledged...", pipe->buffer.count);
        
        if (pthread_cond_timedwait(&pipe->ack_cond, &pipe->ack_lock, &ts) == ETIMEDOUT) {
             WARN("Flush timed out waiting for %zu packets to be acknowledged. Proceeding.", pipe->buffer.count);
             success = false;
             break;
        }
    }
    
    pthread_cond_broadcast(&pipe->ack_cond); 
    pthread_mutex_unlock(&pipe->ack_lock);
    
    if (success) {
        INFO("Pipe successfully flushed. Buffer is empty.");
    }

    return success;
}

void pipe_snd_close(Pipe *pipe)
{
    if (!pipe) return;

    pipe_flush(pipe);
    
    Packet end = PACKET_END;
    if (!pipe_write_packet_sync(pipe, &end, NULL)) {
        WARN("Failed to send END packet synchronously.");
    }
    
    pipe->running = false;
    pipe->retransmit_running = false;

    pthread_cond_broadcast(&pipe->ack_cond);
    
    if (pipe->sock_fd >= 0) {
        close(pipe->sock_fd); 
        pipe->sock_fd = -1;
    }
    
    if (pipe->buffer.count > 0) {
        WARN("There are %zu packets that have not been acknowledged and may be lost.", pipe->buffer.count);
    }
    
    pipe_free(pipe);
    INFO("Pipe sender closed");
}
