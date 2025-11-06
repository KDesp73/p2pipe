#ifndef PIPE_H
#define PIPE_H

#include "p2pipe/buffer.h"
#include "p2pipe/packet.h"
#include "p2pipe/storage.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_CAPACITY 25

typedef enum {
    MODE_SND,
    MODE_RCV
} PipeMode;

typedef struct {
    PipeMode mode;
    Buffer buffer;
    int sock_fd;
    struct sockaddr_in peer_addr;
    Storage storage;
    uint32_t seq;
    uint64_t hash;
    size_t payload_len;

    bool running;
    bool retransmit_running;
    bool handshake_completed;
    bool end_received;

    pthread_mutex_t ack_lock;
    pthread_cond_t ack_cond;
    pthread_mutex_t storage_lock;
    pthread_cond_t storage_cond;
} Pipe;

int pipe_rcv_open(Pipe* pipe, const char* ip, size_t port, size_t capacity);
bool pipe_read(Pipe* pipe, size_t n_bytes);
void pipe_rcv_close(Pipe* pipe);

int pipe_snd_open(Pipe* pipe, const char* ip, size_t port, size_t capacity, void* payload, size_t len);
bool pipe_write(Pipe* pipe, void* payload, size_t len);
bool pipe_flush(Pipe* pipe);
void pipe_snd_close(Pipe* pipe);

#endif // PIPE_H
