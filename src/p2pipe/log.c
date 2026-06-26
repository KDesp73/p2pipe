#include "p2pipe/log.h"
#include <time.h>


void logging_set_file(void)
{
    pthread_mutex_lock(&log_lock);

    struct stat st = {0};
    if (stat("logs", &st) == -1)
        mkdir("logs", 0700);

    time_t now = time(NULL);
    struct tm* t = gmtime(&now);
    if (!t) {
        pthread_mutex_unlock(&log_lock);
        return;
    }

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", t);

    char path[128];
    snprintf(path, sizeof(path), "logs/%s.log", timestamp);

    if (log_file)
        fclose(log_file);

    log_file = fopen(path, "w");
    if (!log_file)
        perror("Failed to open log file");

    pthread_mutex_unlock(&log_lock);
}

FILE* logging_get_file(void)
{
    return log_file ? log_file : stderr;
}
