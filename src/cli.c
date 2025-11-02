#include "cli.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Command parse_command(const char* str)
{
    if(!str) return COMMAND_NONE;

    if(!strcmp("serve", str)) return COMMAND_SERVE;
    else if(!strcmp("listen", str)) return COMMAND_LISTEN;
    else if(!strcmp("talk", str)) return COMMAND_TALK;

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
    ctx->capacity = 0;
    ctx->command = COMMAND_NONE;
    ctx->ip = NULL;
    ctx->src = NULL;
    ctx->dst= NULL;
}

void context_free(Context* ctx)
{
    if(!ctx) return;
    if(ctx->ip) {
        free(ctx->ip);
        ctx->ip = NULL;
    }
    if(ctx->src) {
        free(ctx->src);
        ctx->src = NULL;
    }
    if(ctx->dst) {
        free(ctx->dst);
        ctx->dst = NULL;
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
