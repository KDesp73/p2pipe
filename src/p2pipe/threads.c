#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "p2pipe/threads.h"
#include "extern/logging.h"
#include "p2pipe/log.h"

static void *worker_main(void *vpool)
{
    ThreadPool *p = (ThreadPool *)vpool;

    while (1) {
        pthread_mutex_lock(&p->lock);
        while (!p->stopping && p->task_count == 0) {
            pthread_cond_wait(&p->cond, &p->lock);
        }
        if (p->stopping && p->task_count == 0) {
            pthread_mutex_unlock(&p->lock);
            break;
        }

        Task *t = p->head;
        if (!t) {
            pthread_mutex_unlock(&p->lock);
            continue;
        }
        p->head = t->next;
        if (!p->head) p->tail = NULL;
        p->task_count--;
        pthread_cond_signal(&p->cond);
        pthread_mutex_unlock(&p->lock);

        t->fn(t->arg);
        free(t);
    }

    return NULL;
}

ThreadPool *thread_pool_create(size_t num_threads)
{
    if (num_threads == 0) num_threads = 1;
    ThreadPool *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->n_threads = num_threads;
    p->threads = calloc(num_threads, sizeof(pthread_t));
    if (!p->threads) { free(p); return NULL; }

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->cond, NULL);
    p->head = p->tail = NULL;
    p->task_count = 0;
    p->stopping = false;
    p->stopped = false;

    for (size_t i = 0; i < num_threads; ++i) {
        if (pthread_create(&p->threads[i], NULL, worker_main, p) != 0) {
            p->stopping = true;
            pthread_cond_broadcast(&p->cond);
            for (size_t j = 0; j < i; ++j) pthread_join(p->threads[j], NULL);
            free(p->threads);
            pthread_mutex_destroy(&p->lock);
            pthread_cond_destroy(&p->cond);
            free(p);
            return NULL;
        }
    }
    return p;
}

bool thread_pool_submit(ThreadPool *p, TPTaskFn fn, void *arg)
{
    if (!p || !fn) return false;
    Task *t = malloc(sizeof(*t));
    if (!t) return false;
    t->fn = fn; t->arg = arg; t->next = NULL;

    pthread_mutex_lock(&p->lock);
    if (p->stopping) {
        pthread_mutex_unlock(&p->lock);
        free(t);
        return false;
    }
    if (!p->tail) p->head = t;
    else p->tail->next = t;
    p->tail = t;
    p->task_count++;
    pthread_cond_signal(&p->cond);
    pthread_mutex_unlock(&p->lock);

    return true;
}

void thread_pool_wait(ThreadPool *p)
{
    if (!p) return;

    pthread_mutex_lock(&p->lock);
    while (p->task_count > 0) {
        pthread_cond_wait(&p->cond, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);
}

void thread_pool_join(ThreadPool *p)
{
    if (!p) return;

    if (!p->stopped) {
        thread_pool_shutdown(p);
    }
}

void thread_pool_wake_all(ThreadPool *p)
{
    if (!p) return;
    pthread_mutex_lock(&p->lock);
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);
}


void thread_pool_shutdown(ThreadPool *p) {
    if (!p) return;
    pthread_mutex_lock(&p->lock);
    p->stopping = true;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);

    for (size_t i = 0; i < p->n_threads; ++i) {
        pthread_join(p->threads[i], NULL);
    }

    pthread_mutex_lock(&p->lock);
    Task *t = p->head;
    while (t) {
        Task *n = t->next;
        free(t);
        t = n;
    }
    p->head = p->tail = NULL;
    p->task_count = 0;
    pthread_mutex_unlock(&p->lock);

    p->stopped = true;
}

static bool is_current_thread_in_pool(ThreadPool *p) {
    pthread_t self = pthread_self();
    for (size_t i = 0; i < p->n_threads; ++i) {
        if (pthread_equal(self, p->threads[i])) return true;
    }
    return false;
}

void thread_pool_destroy(ThreadPool *p)
{
    if (!p) return;

    if (is_current_thread_in_pool(p)) {
        ERRO("thread_pool_destroy(): called from a worker thread; refuse to destroy");
        return;
    }

    thread_pool_shutdown(p);
    thread_pool_free(p);
}

void thread_pool_free(ThreadPool *p)
{
    if(!p) return;

    free(p->threads);
    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->cond);
    free(p);
}
