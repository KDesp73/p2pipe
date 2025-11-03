#ifndef METRICS_H
#define METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CSV_HEADER "id,packets_sent,packets_received,packets_lost,packets_acked,acks_lost,threads_used,start,end\n"
#define METRICS_FMT "%s,%zu,%zu,%zu,%zu,%zu,%zu,%lu,%lu\n"
#define METRICS_ARGS(m) (m).id, (m).packets_sent, (m).packets_received, (m).packets_lost, (m).packets_acked, (m).acks_lost, (m).threads_used, (unsigned long)(m).start, (unsigned long)(m).end 

#define METRICS_FILE "metrics.csv"

typedef struct {
    char* id;                // Unique run identifier
    size_t packets_sent;     // Total data packets sent
    size_t packets_received; // Total data packets received (by the receiver)
    size_t packets_lost;     // Estimated data packets lost (sent - acked)
    size_t packets_acked;    // Total data packets acknowledged (by the sender)
    size_t acks_lost;        // Estimated ACK packets lost (sent ACK - received ACK)
    size_t threads_used;     // Number of worker threads used in the thread pool
    uint64_t start;          // Start timestamp (e.g., in microseconds)
    uint64_t end;            // End timestamp (e.g., in microseconds)
} Metrics;

bool metrics_init(Metrics* metrics, const char* path); 
bool metrics_write(const Metrics* metrics, const char* path); 
void metrics_start(Metrics* metrics); 
void metrics_end(Metrics* metrics); 
void metrics_free(Metrics* metrics); 
void metrics_print(const Metrics* metrics); 

extern Metrics metrics;

#endif // METRICS_H
