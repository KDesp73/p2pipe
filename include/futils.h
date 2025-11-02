#ifndef UTILS_H
#define UTILS_H

#include "p2pipe/pipe.h"
#include <stdbool.h>
#include <stddef.h>

bool read_file_bytes(const char *path, void **out_data, size_t *out_len);
bool write_pipe(Pipe* pipe, const char* path);
bool append_packet(const Packet* packet, const char* path);


#endif // UTILS_H
