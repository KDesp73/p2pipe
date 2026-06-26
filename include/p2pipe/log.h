#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <sys/stat.h>
#include <pthread.h>

extern FILE* log_file;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void logging_set_file(void);
FILE* logging_get_file(void);

#ifdef LOG
#undef LOG
#endif

#ifdef DEBUG
#define LOG(stream, type, format, ...)                                      \
    do {                                                                    \
        pthread_mutex_lock(&log_lock);                                      \
        FILE* logf = logging_get_file();                                    \
        fprintf(logf, "[%s] " format "\n", type, ##__VA_ARGS__);            \
        fprintf(stream, "[%s] " format "\n", type, ##__VA_ARGS__);          \
        fflush(logf);                                                       \
        pthread_mutex_unlock(&log_lock);                                    \
    } while(0)
#else
#define LOG(stream, type, format, ...)                                      \
    do {                                                                    \
        pthread_mutex_lock(&log_lock);                                      \
        FILE* logf = logging_get_file();                                    \
        fprintf(logf, "[%s] " format "\n", type, ##__VA_ARGS__);            \
        fflush(logf);                                                       \
        pthread_mutex_unlock(&log_lock);                                    \
    } while(0)
#endif // DEBUG


#endif // LOG_H
