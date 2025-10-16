#include "help.h"
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#define CLI_IMPLEMENTATION
#include "extern/cli.h"
#include "extern/logging.h"
#include "p2p.h"
#include "version.h"
#include "cli.h"

bool serve_handler(Context context) 
{
    if(context.port == -1) {
        ERRO("Please provide a valid port number");
        return false;
    }

    return !server(context.port);
}

bool connect_handler(Context context)
{
    if(!validate_port(context.port)) {
        ERRO("Please provide a valid port number");
        return false;
    }
    if(!context.ip) {
        ERRO("Please provide an ip");
        return false;
    }
    if(!context.id) {
        ERRO("Please provide an id");
        return false;
    }

    return !client(context.ip, context.port, context.id);
}

int main(int argc, char** argv)
{
    cli_args_t args = cli_args_make(
        cli_arg_new(ARG_HELP, "help", "", no_argument),
        cli_arg_new(ARG_VERSION, "version", "", no_argument),
        cli_arg_new(ARG_IP, "ip", "", required_argument),
        cli_arg_new(ARG_PORT, "port", "", required_argument),
        cli_arg_new(ARG_ID, "id", "", required_argument),
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
                printf("p2p v%s\n", VERSION_STRING);
                goto cleanup;
            case ARG_IP:
                ctx.ip = strdup(optarg);
                break;
            case ARG_PORT:
                if(!validate_int(optarg)) {
                    ERRO("Invalid port argument: `%s`", optarg);
                    goto error;
                }
                ctx.port = atoi(optarg);
                break;
            case ARG_ID:
                ctx.id = strdup(optarg);
                break;
            default:
                goto error;
        }
    }

    Dispatcher dispatcher = {0};
    set_handler(&dispatcher, COMMAND_SERVE, serve_handler);
    set_handler(&dispatcher, COMMAND_CONNECT, connect_handler);

    HandlerFunc handler = get_handler(&dispatcher, command);
    if(!handler) {
        if(!command_str) ERRO("Please provide a command");
        else ERRO("Unknown command: `%s`", command_str);
        goto error;
    }
    if(!handler(ctx)) goto error;

cleanup:
    cli_args_free(&args);
    context_free(&ctx);

    return 0;

error:
    cli_args_free(&args);
    context_free(&ctx);
    return 1;
}
