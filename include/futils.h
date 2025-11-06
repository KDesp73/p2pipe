#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

bool read_file_bytes(const char *path, void **out_data, size_t *out_len);
size_t file_size(const char *path);



#endif // UTILS_H
