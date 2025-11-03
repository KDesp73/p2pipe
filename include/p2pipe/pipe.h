#ifndef PIPE_H
#define PIPE_H

#include "p2pipe/packet.h"
#include "p2pipe/storage.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_CAPACITY 25

typedef enum {
    MODE_SND,
    MODE_RCV
} PipeMode;

typedef struct {
    PipeMode mode;
    Packet* buffer;
    size_t count;
    size_t capacity;
    int sock_fd;
    struct sockaddr_in peer_addr;
    Storage storage;
} Pipe;

int pipe_rcv_open(Pipe* pipe, const char* ip, size_t port, size_t capacity);
bool pipe_read(Pipe* pipe, size_t n_bytes, const char* dst);
void pipe_rcv_close(Pipe* pipe);

int pipe_snd_open(Pipe* pipe, const char* ip, size_t port, size_t capacity);
bool pipe_write(Pipe* pipe, void* payload, size_t len);
bool pipe_flush(Pipe* pipe);
void pipe_snd_close(Pipe* pipe);

#endif // PIPE_H
