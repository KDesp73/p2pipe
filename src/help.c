#include "help.h"
#include "extern/ansi.h"
#include <stdio.h>

#define PTN(format, ...) \
    do { \
        fprintf(stdout, format, ##__VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while(0)

// With 2 space indent
#define PTNI(format, ...) \
    do { \
        PTN("  "format, ##__VA_ARGS__); \
    } while(0)


void help()
{
    PTN("%sUSAGE%s", ANSI_BOLD, ANSI_RESET);
    PTNI("p2p <COMMAND> <OPTIONS>...");
    PTN(" ");

    PTN("%sCOMMANDS%s", ANSI_BOLD, ANSI_RESET);
    PTNI("serve             Tells the executable to act as a server");
    PTNI("connect           Connect to a server");

    PTN(" ");

    PTN("%sOPTIONS%s", ANSI_BOLD, ANSI_RESET);
    PTNI("-h --help         Prints this message");
    PTNI("-v --version      Prints the project version");

    PTN(" ");
    PTN("Written by KDesp73 (Konstantinos Despoinidis)");
}

