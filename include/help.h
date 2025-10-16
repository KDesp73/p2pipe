#ifndef HELP_H
#define HELP_H

#include "cli.h"

void help();

static inline void run_help(Command command)
{
    switch (command) {
    case COMMAND_SERVE:
    case COMMAND_CONNECT:
    case COMMAND_NONE:
        help();
      break;
    }
}


#endif // HELP_H
