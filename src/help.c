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
    PTNI("p2pipe <COMMAND> <OPTIONS>...");
    PTN(" ");

    PTN("%sCOMMANDS%s", ANSI_BOLD, ANSI_RESET);
    PTNI("serve             Act as a server");
    PTNI("listen            Act as a receiver");
    PTNI("talk              Act as a sender");

    PTN(" ");

    PTN("%sOPTIONS%s", ANSI_BOLD, ANSI_RESET);
    PTNI("-h --help         Prints this message");
    PTNI("-v --version      Prints the project version");
    PTNI("-I --ip           Specify the ip of the server");
    PTNI("-P --port         Specify the port of the server");
    PTNI("-s --src          Specify the source file");
    PTNI("-d --dst          Specify the destination file");
    PTNI("-C --capacity     Specify the buffer capacity");
    PTNI("-i --id           Specify the session id");

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

#define client_help(cmd) \
    PTN("%sUSAGE%s", ANSI_BOLD, ANSI_RESET); \
    PTNI("p2p %s <OPTIONS>...", cmd); \
    PTN(" "); \
    PTN("%sOPTIONS%s", ANSI_BOLD, ANSI_RESET); \
    PTNI("-h --help         Prints this message"); \
    PTNI("-I --ip           Specify the ip of the server"); \
    PTNI("-P --port         Specify the port of the server"); \
    PTNI("-C --capacity     Specify the buffer capacity")

void talk_help() {
    client_help("talk");
    PTNI("-s --src          Specify the source file");
}

void listen_help() {
    client_help("listen");
    PTNI("-d --dst          Specify the destination file");
    PTNI("-i --id           Specify the session id");
}
