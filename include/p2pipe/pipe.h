#ifndef PIPE_H
#define PIPE_H

#include "p2pipe/packet.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MODE_SND,
    MODE_RCV
} PipeMode;

typedef struct {
    PipeMode mode;
    Packet* buffer;
    size_t size;
    size_t capacity;
} Pipe;

int pipe_rcv_open(Pipe* pipe, size_t capacity);
bool pipe_read(Pipe* pipe, size_t n);
void pipe_rcv_close(Pipe* pipe);

int pipe_snd_open(Pipe* pipe, size_t capacity);
bool pipe_write(Pipe* pipe, char* buf, size_t len);
bool pipe_flush(Pipe* pipe);
void pipe_snd_close(Pipe* pipe);

#endif // PIPE_H
