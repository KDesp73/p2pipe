#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*TPTaskFn)(void *arg);
typedef struct task {
    TPTaskFn fn;
    void *arg;
    struct task *next;
} Task;

typedef struct {
    pthread_t *threads;
    size_t n_threads;

    pthread_mutex_t lock;
    pthread_cond_t cond;
    Task *head;
    Task *tail;
    size_t task_count;

    bool stopping;
    bool stopped;
} ThreadPool;

ThreadPool *thread_pool_create(size_t num_threads);
bool thread_pool_submit(ThreadPool *p, TPTaskFn fn, void *arg);
void thread_pool_wait(ThreadPool *p);
void thread_pool_join(ThreadPool *p);
void thread_pool_wake_all(ThreadPool *p);
void thread_pool_shutdown(ThreadPool *p);
void thread_pool_destroy(ThreadPool *p);
void thread_pool_free(ThreadPool *p);

extern ThreadPool* tp;

#endif // THREADS_H
