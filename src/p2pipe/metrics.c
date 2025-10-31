#include "p2pipe/metrics.h"
#include "extern/logging.h"
#include <stdio.h>
#include <string.h>

bool metrics_init(const char* path)
{
    // TODO: check if file already exists
    FILE* fd = fopen(path, "w");
    if(!fd) {
        ERRO("Could not open file `%s` for writing", path);
        return false;
    }

    if(!fwrite(CSV_HEADER, sizeof(CSV_HEADER), 1, fd)) {
        ERRO("Failed writing csv header");
        return false;
    }

    fclose(fd);
    return true;
}

bool metrics_write(Metrics metrics, const char* path)
{
    FILE* fd = fopen(path, "a");
    if(!fd) {
        ERRO("Could not open file `%s` for writing", path);
        return false;
    }
    
    char buffer[1024];
    sprintf(buffer, METRICS_FMT, METRICS_ARGS(metrics));
    if(!fwrite(buffer, strlen(buffer), 1, fd)) {
        ERRO("Could not write record");
        return false;
    }

    fclose(fd);
    return true;
}

