#ifndef HELPERS_H
#define HELPERS_H

#include "p2pipe/pipe.h"
#include <stdbool.h>

void pipe_init(Pipe* pipe, size_t capacity);
void pipe_free(Pipe* pipe);

bool pipe_write_packet_sync(Pipe* pipe, const Packet* packet, struct sockaddr_in *dest);
bool pipe_write_packet_async(Pipe* pipe, const Packet* packet, struct sockaddr_in *dest);

void packet_listener(void* arg);
void retransmission_thread(void* arg);

long long current_time_ms();

bool threads_init(void);
void threads_shutdown(void);

void send_job_fn(void *arg);
void recv_job_fn(void *arg);

uint64_t compute_fnv1a_hash(const void* payload, size_t len);

typedef struct {
    int sock_fd;
    struct sockaddr_in peer;
    Packet packet;
    Pipe *pipe;
} SendJob;

typedef struct {
    Pipe *pipe;
    Packet *packet;
    struct sockaddr_in src;
} RecvJob;


#endif // HELPERS_H
