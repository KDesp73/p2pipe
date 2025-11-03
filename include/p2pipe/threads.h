#ifndef THREADS_H
#define THREADS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct thread_pool ThreadPool;
typedef void (*tp_task_fn)(void *arg);

ThreadPool *thread_pool_create(size_t num_threads);
bool thread_pool_submit(ThreadPool *p, tp_task_fn fn, void *arg);
void thread_pool_shutdown(ThreadPool *p);
void thread_pool_destroy(ThreadPool *p);

typedef struct ordered_executor ordered_executor_t;
typedef void (*oe_task_fn)(void *arg);

ordered_executor_t *ordered_executor_create(ThreadPool *pool, size_t capacity_hint);
bool ordered_executor_submit(ordered_executor_t *oe, uint64_t key, oe_task_fn fn, void *arg);
void ordered_executor_shutdown(ordered_executor_t *oe);
void ordered_executor_destroy(ordered_executor_t *oe);

#endif // THREADS_H
