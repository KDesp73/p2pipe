#ifndef ID_H
#define ID_H

#include <stddef.h>
#include <stdlib.h>

#define BASE56_LEN 16

static const char base56_chars[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static inline void generate_base56_id(char* out, size_t len) {
    if (!out || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        int idx = rand() % (sizeof(base56_chars) - 1);
        out[i] = base56_chars[idx];
    }
    out[len] = '\0';
}

#endif // ID_H
