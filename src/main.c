#include "futils.h"
#include "help.h"
#include "futils.h"
#include "p2pipe/log.h"
#include "p2pipe/metrics.h"
#include "p2pipe/packet.h"
#include "p2pipe/storage.h"
#include "validation.h"
#include "p2pipe/bootstrap.h"
#include <bits/getopt_core.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CLI_IMPLEMENTATION
#include "extern/cli.h"
#include "extern/logging.h"
#include "p2pipe/log.h"
#include "p2pipe/pipe.h"
#include "p2pipe/version.h"
#include "cli.h"

Metrics metrics;
FILE* log_file = NULL;

bool serve_handler(Context context) 
{
    if(!validate_port(context.port)) {
        ERRO("Please provide a valid port number");
        return false;
    }

    return !server(context.port);
}

bool listen_handler(Context context)
{
    if (!validate_port(context.port)) {
        ERRO("Please provide a valid port number");
        return false;
    }
    if (!context.id) {
        ERRO("Please provide the session id");
        return false;
    }
    if (!context.ip) {
        ERRO("Please provide an ip");
        return false;
    }
    if (!context.dst) {
        ERRO("Please specify a destination file");
        return false;
    }

    size_t capacity = context.capacity ? context.capacity : DEFAULT_CAPACITY;
    METRICS_SET(buffer_capacity, capacity);

    Pipe pipe = {0};
    int sock = pipe_rcv_open(&pipe, context.ip, context.port, context.id, capacity, NULL);
    if (sock <= 0) {
        return false;
    }

    storage_init(&pipe.storage, capacity, context.dst, pipe.seq); 
    pipe.storage.stream_data = true;

    INFO("Listening for packets...");
    metrics_start(&metrics);

    if (!pipe_read(&pipe, PACKET_BUFFER_SIZE * pipe.buffer.capacity)) {
        ERRO("Could not read packets");
        pipe_rcv_close(&pipe);
        return false;
    }

    pipe_rcv_close(&pipe);

    INFO("Exported received data to '%s'", context.dst);
    return true;
}

#define LARGE_FILE_THRESHOLD (100 * 1024 * 1024)
#define CHUNK_SIZE (1024 * 1024)

bool stream_file(const char* path, Pipe* pipe) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        ERRO("Failed to open file '%s'", path);
        return false;
    }

    void* buf = malloc(CHUNK_SIZE);
    if (!buf) {
        ERRO("Failed to allocate buffer");
        fclose(f);
        return false;
    }

    size_t nread;
    while ((nread = fread(buf, 1, CHUNK_SIZE, f)) > 0) {
        if (!pipe_write(pipe, buf, nread)) {
            ERRO("Write failed while streaming '%s'", path);
            free(buf);
            fclose(f);
            return false;
        }
    }

    free(buf);
    fclose(f);
    return true;
}

bool talk_handler(Context context)
{
    if (!validate_port(context.port)) {
        ERRO("Please provide a valid port number");
        return false;
    }
    if (!context.ip) {
        ERRO("Please provide an ip");
        return false;
    }
    if (!context.src) {
        ERRO("Please specify a source file");
        return false;
    }

    size_t capacity = context.capacity ? context.capacity : DEFAULT_CAPACITY;
    METRICS_SET(buffer_capacity, capacity);

    Pipe pipe = {0};
    int sock = pipe_snd_open(&pipe, context.ip, context.port, capacity, NULL);
    if (sock <= 0) {
        pipe_snd_close(&pipe);
        return false;
    }
    metrics_start(&metrics);

    bool ok;
    size_t size = file_size(context.src);
    pipe.payload_len = size;
    METRICS_SET(payload_len, size);
    if (size > LARGE_FILE_THRESHOLD) {
        ok = stream_file(context.src, &pipe);
    } else {
        void* buffer = NULL;
        size_t len = 0;
        if (!read_file_bytes(context.src, &buffer, &len)) {
            ERRO("Failed to read source file: %s", context.src);
            pipe_snd_close(&pipe);
            return false;
        }
        ok = pipe_write(&pipe, buffer, len);
        free(buffer);
    }

    pipe_snd_close(&pipe);
    return ok;
}

void sigint_handler(int sig)
{
    metrics_end(&metrics);
    metrics_write(&metrics, METRICS_FILE);
    metrics_free(&metrics);
    exit(0);
}

int main(int argc, char** argv)
{
#ifdef METRICS_ENABLED
    printf("[DEBU] Metrics enabled\n");
#endif
    signal(SIGINT, sigint_handler);
    logging_set_file();

    metrics_init(&metrics, METRICS_FILE);

    cli_args_t args = cli_args_make(
        cli_arg_new(ARG_HELP, "help", "", no_argument),
        cli_arg_new(ARG_VERSION, "version", "", no_argument),
        cli_arg_new(ARG_IP, "ip", "", required_argument),
        cli_arg_new(ARG_ID, "id", "", required_argument),
        cli_arg_new(ARG_PORT, "port", "", required_argument),
        cli_arg_new(ARG_CAPACITY, "capacity", "", required_argument),
        cli_arg_new(ARG_SRC, "src", "", required_argument),
        cli_arg_new(ARG_DST, "dst", "", required_argument),
        NULL
    );
    char* command_str = argc == 1 ? NULL : argv[1];
    Command command = parse_command(command_str);

    Context ctx = {0};
    context_init(&ctx, command);

    int opt;
    LOOP_ARGS(opt, args) {
        switch (opt) {
            case ARG_HELP:
                run_help(command);
                goto cleanup;
            case ARG_VERSION:
                printf("p2pipe v%s\n", VERSION_STRING);
                goto cleanup;
            case ARG_IP:
                ctx.ip = strdup(optarg);
                break;
            case ARG_ID:
                ctx.id = strdup(optarg);
                break;
            case ARG_PORT:
                if(!validate_int(optarg)) {
                    ERRO("Invalid port argument: `%s`", optarg);
                    goto error;
                }
                ctx.port = atoi(optarg);
                break;
            case ARG_CAPACITY:
                if(!validate_int(optarg) && atoi(optarg) <= 0) {
                    ERRO("Invalid capacity: `%s`", optarg);
                    goto error;
                }
                ctx.capacity = atoi(optarg);
                break;
            case ARG_SRC:
                ctx.src = strdup(optarg);
                break;
            case ARG_DST:
                ctx.dst = strdup(optarg);
                break;
            default:
                goto error;
        }
    }
    
    Dispatcher dispatcher = {0};
    set_handler(&dispatcher, COMMAND_SERVE, serve_handler);
    set_handler(&dispatcher, COMMAND_LISTEN, listen_handler);
    set_handler(&dispatcher, COMMAND_TALK, talk_handler);

    HandlerFunc handler = get_handler(&dispatcher, command);
    if(!handler) {
        if(!command_str) ERRO("Please provide a command");
        else ERRO("Unknown command: `%s`", command_str);
        goto error;
    }
    if(!handler(ctx)) goto error;

    metrics_end(&metrics);
    // metrics_print(&metrics);
    metrics_write(&metrics, METRICS_FILE);

cleanup:
    metrics_free(&metrics);
    cli_args_free(&args);
    context_free(&ctx);
    return 0;

error:
    metrics_free(&metrics);
    cli_args_free(&args);
    context_free(&ctx);
    return 1;
}
