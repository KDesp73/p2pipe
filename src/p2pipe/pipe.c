#include "p2pipe/pipe.h"
#include "extern/logging.h"
#include "p2pipe/packet.h"
#include <stdlib.h>

static void pipe_init(Pipe* pipe, size_t capacity)
{
    pipe->buffer = malloc(sizeof(Packet) * capacity);
    if(!pipe->buffer) {
        ERRO("Could not allocate buffer");
        return;
    }
    pipe->capacity = capacity;
}

static void pipe_free(Pipe* pipe)
{
    free(pipe->buffer);
}

int pipe_rcv_open(Pipe* pipe, size_t capacity)
{
    pipe_init(pipe, capacity);
    pipe->mode = MODE_RCV;

    return 0; // TODO: return descriptor
}

bool pipe_read(Pipe* pipe, size_t n)
{
    return false;
}

void pipe_rcv_close(Pipe* pipe)
{
    pipe_free(pipe);
}

int pipe_snd_open(Pipe* pipe, size_t capacity)
{
    pipe_init(pipe, capacity);
    pipe->mode = MODE_SND;

    return 0; // TODO: return descriptor
}

bool pipe_write(Pipe* pipe, char* buf, size_t len)
{
    return false;
}

bool pipe_flush(Pipe* pipe)
{
    return false;
}

void pipe_snd_close(Pipe* pipe)
{
    pipe_free(pipe);
}

