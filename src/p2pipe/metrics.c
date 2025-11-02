#include "p2pipe/metrics.h"
#include "extern/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

bool metrics_init(Metrics* metrics, const char* path)
{
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

    metrics->id = NULL;
    metrics->packets_sent = 0;
    metrics->packets_received = 0;
    metrics->packets_lost = 0;
    metrics->acks_lost = 0;
    metrics->threads_used = 1;

    return true;
}

bool metrics_write(const Metrics* metrics, const char* path)
{
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
    metrics->start = time(NULL);
}

void metrics_end(Metrics* metrics)
{
    metrics->end = time(NULL);
}

void metrics_free(Metrics* metrics)
{
    if(!metrics) return;
    if(metrics->id){
        free(metrics->id);
        metrics->id = NULL;
    }
}
