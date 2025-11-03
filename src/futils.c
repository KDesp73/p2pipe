#include "futils.h"
#include <stdio.h>
#include <stdlib.h>

bool read_file_bytes(const char *path, void **out_data, size_t *out_len)
{
    if (!path || !out_data || !out_len)
        return false;

    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size < 0) {
        perror("ftell");
        fclose(f);
        return false;
    }
    rewind(f);

    void *buffer = malloc((size_t)size);
    if (!buffer) {
        perror("malloc");
        fclose(f);
        return false;
    }

    size_t read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        fprintf(stderr, "read_file_bytes: incomplete read (%zu/%zu)\n", read, (size_t)size);
        free(buffer);
        return false;
    }

    *out_data = buffer;
    *out_len = (size_t)size;
    return true;
}
