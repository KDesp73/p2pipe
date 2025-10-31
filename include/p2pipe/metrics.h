#ifndef METRICS_H
#define METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CSV_HEADER "packets_sent, packets_received, packets_lost, acks_lost, threads_used, start, end"
#define METRICS_FMT "%zu, %zu, %zu, %zu, %zu, %lu, %lu"
#define METRICS_ARGS(m) m.packets_sent, m.packets_received, m.packets_lost, m.acks_lost, m.threads_used, m.start, m.end 

typedef struct {
    size_t packets_sent;
    size_t packets_received;
    size_t packets_lost;
    size_t acks_lost;
    size_t threads_used;
    uint64_t start;
    uint64_t end;
} Metrics;

bool metrics_init(const char* path);
bool metrics_write(Metrics metrics, const char* path);

#endif // METRICS_H
