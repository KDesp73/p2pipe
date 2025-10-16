#include "cli.h"
#include <stdlib.h>
#include <string.h>

Command parse_command(const char* str)
{
    if(!str) return COMMAND_NONE;

    if(!strcmp("serve", str)) return COMMAND_SERVE;
    else if(!strcmp("connect", str)) return COMMAND_CONNECT;

    return COMMAND_NONE;
}

void context_init(Context* ctx, Command command)
{
    context_reset(ctx);
    ctx->command = command;
}

void context_reset(Context* ctx)
{
    context_free(ctx);
    ctx->port = -1;
    ctx->command = COMMAND_NONE;
    ctx->ip = NULL;
    ctx->id = NULL;
}

void context_free(Context* ctx)
{
    if(!ctx) return;
    if(ctx->ip) {
        free(ctx->ip);
        ctx->ip = NULL;
    }
    if(ctx->id) {
        free(ctx->id);
        ctx->id = NULL;
    }
}

void set_handler(Dispatcher* this, Command command, HandlerFunc handler)
{
    this->table[command] = handler;
}

HandlerFunc get_handler(Dispatcher* this, Command command)
{
    if(command == COMMAND_NONE) return NULL;
    return this->table[command];
}
