#ifndef HELPERS_H
#define HELPERS_H

#include "p2pipe/pipe.h"
#include <stdbool.h>


long long current_time_ms();
uint64_t current_time_us();

bool threads_init(void);
void threads_shutdown(void);

void packet_listener(void* arg);
void retransmission_thread(void* arg);
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
