#ifndef CLI_CONFIG_H
#define CLI_CONFIG_H

#include <stdbool.h>

typedef enum {
    COMMAND_SERVE,
    COMMAND_CONNECT,
    COMMAND_NONE,
} Command;
#define COMMAND_COUNT COMMAND_NONE
Command parse_command(const char* str);

typedef enum {
   ARG_HELP = 'h',
   ARG_VERSION = 'v',
   ARG_IP = 'I',
   ARG_PORT = 'P',
   ARG_ID = 'i'
} CliArgs;

typedef struct {
    char* ip;
    int port;
    char* id;
    Command command;
} Context;
void context_init(Context* ctx, Command command);
void context_reset(Context* ctx);
void context_free(Context* ctx);

typedef bool (*HandlerFunc)(Context);
typedef struct {
   HandlerFunc table[COMMAND_COUNT];
} Dispatcher;
void set_handler(Dispatcher* this, Command command, HandlerFunc handler);
HandlerFunc get_handler(Dispatcher* this, Command command);

#endif // CLI_CONFIG_H
