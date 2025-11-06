#ifndef THREADS_H
#define THREADS_H

#include <bits/pthreadtypes.h>
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
void thread_pool_shutdown(ThreadPool *p);
void thread_pool_destroy(ThreadPool *p);
void thread_pool_wake_all(ThreadPool *p);


typedef void (*OETaskFn)(void *arg);
typedef struct oe_task {
    OETaskFn fn;
    void *arg;
    struct oe_task *next;
} OETask;

typedef struct key_entry {
    uint64_t key;
    OETask *head;
    OETask *tail;
    bool active;
    struct key_entry *next;
} KeyEntry;

typedef struct {
    ThreadPool *pool;

    pthread_mutex_t lock;
    size_t table_size;
    KeyEntry **table;

    bool shutting_down;
} OrderedExecutor;

OrderedExecutor *ordered_executor_create(ThreadPool *pool, size_t capacity_hint);
bool ordered_executor_submit(OrderedExecutor *oe, uint64_t key, OETaskFn fn, void *arg);
void ordered_executor_shutdown(OrderedExecutor *oe);
void ordered_executor_destroy(OrderedExecutor *oe);

extern ThreadPool* tp;
extern OrderedExecutor* oe;

#endif // THREADS_H
