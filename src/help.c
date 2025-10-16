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
    PTNI("-I --ip           Specify the ip of the server");
    PTNI("-P --port         Specify the port of the server");
    PTNI("-i --id           Specify the peer's id");

    PTN(" ");
    PTN("Written by KDesp73 (Konstantinos Despoinidis)");
}

void serve_help()
{
    PTN("%sUSAGE%s", ANSI_BOLD, ANSI_RESET);
    PTNI("p2p serve <OPTIONS>...");
    PTN(" ");

    PTN("%sOPTIONS%s", ANSI_BOLD, ANSI_RESET);
    PTNI("-h --help         Prints this message");
    PTNI("-P --port         Specify the port of the server");
}

void connect_help()
{
    PTN("%sUSAGE%s", ANSI_BOLD, ANSI_RESET);
    PTNI("p2p connect <OPTIONS>...");
    PTN(" ");

    PTN("%sOPTIONS%s", ANSI_BOLD, ANSI_RESET);
    PTNI("-h --help         Prints this message");
    PTNI("-I --ip           Specify the ip of the server");
    PTNI("-P --port         Specify the port of the server");
    PTNI("-i --id           Specify the peer's id");
}
