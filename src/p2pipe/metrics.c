#include "p2pipe/metrics.h"
#include "extern/logging.h"
#include "p2pipe/log.h"
#include "p2pipe/helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

void metrics_reset(Metrics* metrics)
{
    memset(metrics, 0, sizeof(Metrics));
    metrics->type = -1;
}

bool metrics_init(Metrics* metrics, const char* path)
{
#ifndef METRICS_ENABLED
    printf("Metrics are disabled\n");
    return false;
#endif

    if (access(path, F_OK) == 0)
        return true; // File exists

    FILE* fd = fopen(path, "w");
    if (!fd) {
        ERRO("Could not open file `%s` for writing", path);
        return false;
    }

    if (fwrite(CSV_HEADER, 1, strlen(CSV_HEADER), fd) != strlen(CSV_HEADER)) {
        ERRO("Failed writing CSV header");
        fclose(fd);
        return false;
    }

    fclose(fd);

    metrics_reset(metrics);

    return true;
}

bool metrics_write(const Metrics* metrics, const char* path)
{
#ifndef METRICS_ENABLED
    return false;
#endif
    FILE* fd = fopen(path, "a");
    if (!fd) {
        ERRO("Could not open file `%s` for writing", path);
        return false;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), METRICS_FMT, METRICS_ARGS(*metrics));

    size_t len = strlen(buffer);
    if (fwrite(buffer, 1, len, fd) != len) {
        ERRO("Could not write record");
        fclose(fd);
        return false;
    }

    fclose(fd);
    return true;
}

void metrics_start(Metrics* metrics)
{
#ifndef METRICS_ENABLED
    return;
#endif
    metrics->start = current_time_us();
}

void metrics_end(Metrics* metrics)
{
#ifndef METRICS_ENABLED
    return;
#endif
    metrics->end = current_time_us();
}

void metrics_free(Metrics* metrics)
{
    if(!metrics) return;
    if(metrics->id){
        free(metrics->id);
        metrics->id = NULL;
    }
    metrics_reset(metrics);
}

void metrics_print(const Metrics* metrics)
{
#ifndef METRICS_ENABLED
    return;
#endif
    uint64_t duration_us = metrics->end > metrics->start ? metrics->end - metrics->start : 0;
    double duration_ms = (double)duration_us / 1000.0;

    printf("\n## Protocol Metrics Report\n");
    printf("\n**Session ID:** %s\n", metrics->id ? metrics->id : "N/A");

    printf("\n### Time & Performance\n");
    printf("- Start Time (us): %lu\n", (unsigned long)metrics->start);
    printf("- End Time (us) | %lu\n", (unsigned long)metrics->end);
    printf("- **Duration (ms)**:  **%.3f ms**\n", duration_ms);
    
    printf("\n### Packet Statistics\n");
    printf("- Packets Sent: %zu\n", metrics->packets_sent);
    printf("- Packets Received: %zu\n", metrics->packets_received);
    printf("- **Estimated Packets Lost**: **%zu**\n", metrics->packets_lost);
    printf("- Estimated ACKs Lost: %zu\n", metrics->acks_lost);
}
