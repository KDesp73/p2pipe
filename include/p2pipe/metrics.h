#ifndef METRICS_H
#define METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CSV_HEADER "id, packets_sent, packets_received, packets_lost, acks_lost, threads_used, start, end\n"
#define METRICS_FMT "%s, %zu, %zu, %zu, %zu, %zu, %lu, %lu\n"
#define METRICS_ARGS(m) (m).id, (m).packets_sent, (m).packets_received, (m).packets_lost, (m).acks_lost, (m).threads_used, (m).start, (m).end 

#define METRICS_FILE "metrics.csv"

typedef struct {
    char* id;
    size_t packets_sent;
    size_t packets_received;
    size_t packets_lost;
    size_t acks_lost;
    size_t threads_used;
    uint64_t start;
    uint64_t end;
} Metrics;

bool metrics_init(Metrics* metrics, const char* path);
bool metrics_write(const Metrics* metrics, const char* path);
void metrics_start(Metrics* metrics);
void metrics_end(Metrics* metrics);
void metrics_free(Metrics* metrics);

extern Metrics metrics;

#endif // METRICS_H
