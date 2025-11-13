// receiver.c
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
#include "p2pipe/pipe.h"
#include "common.h"
#include "p2pipe/storage.h"

// Global definitions (needed by the library)
Metrics metrics;
FILE *log_file = NULL;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <id> <expected_N> <repeats>\n", prog);
    fprintf(stderr, "Example: %s ukWhu4vgPg75L9gs 1000 3\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigint_handler);
    logging_set_file();

    if (argc < 4)
        usage(argv[0]);

    const char *id = argv[1];
    size_t expected_N = (size_t)atoll(argv[2]);
    int repeats = atoi(argv[3]);
    int rc = 0;

    Pipe pipe = {0};
    int fd = pipe_rcv_open(&pipe, SERVER_IP, SERVER_PORT, id, CAPACITY, NULL);
    if (fd <= 0) {
        ERRO("Could not open pipe for reading");
        pipe_rcv_close(&pipe);
        return 1;
    }

    for (size_t i = 0; i < repeats; i++) {
        storage_init(&pipe.storage, CAPACITY * 2, "received.rcv", pipe.seq);
        pipe.storage.stream_data = true;
        if (!metrics_init(&metrics, METRICS_FILE))
            WARN("Could not initialize metrics");

        METRICS_SET(buffer_capacity, CAPACITY);
        METRICS_SET(payload_len, PAYLOAD_SIZE * expected_N);

        metrics_start(&metrics);
        if (!pipe_read(&pipe, PAYLOAD_SIZE * expected_N)) {
            ERRO("Error while reading packets");
            rc = 1;
        }

        metrics_end(&metrics);
        metrics_write(&metrics, METRICS_FILE);
        metrics_free(&metrics);
        metrics.id = strdup(id);
        storage_free(&pipe.storage);

        if (rc)
            break;
    }

    pipe_rcv_close(&pipe);
    return rc;
}
