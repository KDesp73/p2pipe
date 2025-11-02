#include "futils.h"
#include "extern/logging.h"
#include "p2pipe/pipe.h"
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

bool write_pipe(Pipe* pipe, const char* path)
{
    if (!pipe || !path) {
        ERRO("Invalid arguments to write_pipe()");
        return false;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return false;
    }

    size_t total_written = 0;

    for (size_t i = 0; i < pipe->count; i++) {
        size_t bytes_to_write = PACKET_BUFFER_SIZE;
        size_t written = fwrite(pipe->buffer[i].data, 1, bytes_to_write, f);
        if (written < bytes_to_write && ferror(f)) {
            perror("fwrite");
            fclose(f);
            return false;
        }
        total_written += written;
    }

    fclose(f);

    INFO("Wrote %zu packets (%zu bytes total) to '%s'",
         pipe->count, total_written, path);

    return true;
}

bool append_packet(const Packet* packet, const char* path)
{
    if (!packet || !path) return false;

    FILE* f = fopen(path, "ab");
    if (!f) {
        perror("fopen");
        return false;
    }
    
    size_t written = fwrite(packet->data, 1, packet->len, f);
    if (written != packet->len) {
        ERRO("Could not write entire packet");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}
