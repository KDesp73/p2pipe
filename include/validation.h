#ifndef VALIDATION_H
#define VALIDATION_H

#include <stdbool.h>
#include <ctype.h>

static inline bool validate_int(char* str) {
    if (!str || *str == '\0') return false;

    char *p = str;

    if (*p == '+' || *p == '-') p++;

    if (!isdigit(*p)) return false;

    for (; *p != '\0'; p++) {
        if (!isdigit(*p)) return false;
    }

    return true;
}

static inline bool validate_port(int port) {
    return port > 1000;
}

#endif // VALIDATION_H
