#ifndef HELP_H
#define HELP_H

#include "cli.h"

void help();
void serve_help();
void listen_help();
void talk_help();

static inline void run_help(Command command)
{
    switch (command) {
    case COMMAND_SERVE:
        serve_help();
        break;
    case COMMAND_LISTEN:
        listen_help();
        break;
    case COMMAND_TALK:
        talk_help();
        break;
    case COMMAND_NONE:
    default:
        help();
      break;
    }
}


#endif // HELP_H
