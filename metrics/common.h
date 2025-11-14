#ifndef COMMON_H
#define COMMON_H

#include "p2pipe/metrics.h"
#include <stdlib.h>
#include <time.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define PAYLOAD_SIZE 512 * 2
#define CAPACITY 25

#undef METRICS_FILE
#define METRICS_FILE "metrics/metrics.csv"

static inline double timespec_diff_seconds(const struct timespec *start, const struct timespec *end)
{
    double s = (double)(end->tv_sec - start->tv_sec);
    double ns = (double)(end->tv_nsec - start->tv_nsec) / 1e9;
    return s + ns;
}

static inline void sigint_handler(int sig)
{
    metrics_end(&metrics);
    metrics_write(&metrics, METRICS_FILE);
    metrics_free(&metrics);
    exit(0);
}

#endif // COMMON_H
