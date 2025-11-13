// sender.c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "extern/logging.h"
#include "p2pipe/log.h"
#include "p2pipe/metrics.h"
#include "p2pipe/packet.h"
#include "p2pipe/pipe.h"
#include "common.h"

// Global definitions (needed by the library)
Metrics metrics;
FILE *log_file = NULL;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <N> <repeats>\n", prog);
    fprintf(stderr, "Example: %s 1000 3\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigint_handler);
    logging_set_file();

    if (argc < 3)
        usage(argv[0]);

    size_t N = (size_t)atoll(argv[1]);
    int repeats = atoi(argv[2]);
    int rc = 0;

    char* id = NULL;

    uint8_t payload[PAYLOAD_SIZE];
    for (size_t i = 0; i < PAYLOAD_SIZE; ++i)
        payload[i] = (uint8_t)(i & 0xFF);

    Pipe pipe = {0};
    int fd = pipe_snd_open(&pipe, SERVER_IP, SERVER_PORT, CAPACITY, NULL);
    if (fd <= 0) {
        ERRO("Could not open pipe for writing");
        pipe_snd_close(&pipe);
        return 1;
    }

    id = strdup(metrics.id);

    for (size_t i = 0; i < repeats; i++) {
        pipe.seq = 0;
        if (!metrics_init(&metrics, METRICS_FILE))
            WARN("Could not initialize metrics");

        metrics_start(&metrics);
        METRICS_SET(buffer_capacity, CAPACITY);
        METRICS_SET(payload_len, PAYLOAD_SIZE * N);

        for (size_t j = 0; j < N; j++) {
            if (!pipe_write(&pipe, payload, sizeof(payload))) {
                ERRO("Error while writing packet");
                rc = 1;
                break;
            }
        }

        metrics_end(&metrics);
        metrics_write(&metrics, METRICS_FILE);
        metrics_free(&metrics);
        metrics.id = strdup(id);

        if (rc)
            break;
    }

    free(id);
    pipe_snd_close(&pipe);
    return rc;
}
