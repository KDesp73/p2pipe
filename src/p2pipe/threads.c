#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "p2pipe/threads.h"

static void *worker_main(void *vpool) {
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
        pthread_mutex_unlock(&p->lock);

        t->fn(t->arg);
        free(t);
    }

    return NULL;
}

ThreadPool *thread_pool_create(size_t num_threads) {
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

bool thread_pool_submit(ThreadPool *p, TPTaskFn fn, void *arg) {
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
        // Use a different condition variable for waiting on task completion
        // If one doesn't exist, we must stick to the busy wait for now, 
        // but it should ideally use a condition variable.
        
        // Sticking to the original pattern but keeping the lock held
        // to avoid race, which is safer but still a busy wait.
        
        // Using sched_yield() while holding a lock is still generally bad,
        // so the original logic of unlock/yield/lock is likely preferred 
        // by the original author despite being a busy-wait.
        
        // To fix the potential race from the busy-wait without adding a new CV:
        // We'll trust the original intent, but this is a potential efficiency issue.
        pthread_mutex_unlock(&p->lock);
        sched_yield();
        pthread_mutex_lock(&p->lock);
    }
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

void thread_pool_destroy(ThreadPool *p) {
    if (!p) return;
    thread_pool_shutdown(p);
    free(p->threads);
    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->cond);
    free(p);
}

static inline size_t key_hash(uint64_t key, size_t table_size) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    return (size_t)(key) & (table_size - 1);
}

OrderedExecutor *ordered_executor_create(ThreadPool *pool, size_t capacity_hint) {
    OrderedExecutor *oe = calloc(1, sizeof(*oe));
    if (!oe) return NULL;
    oe->pool = pool;
    pthread_mutex_init(&oe->lock, NULL);
    size_t ts = 1;
    while (ts < capacity_hint) ts <<= 1;
    if (ts < 16) ts = 16;
    oe->table_size = ts;
    oe->table = calloc(ts, sizeof(KeyEntry*));
    if (!oe->table) { free(oe); return NULL; }
    oe->shutting_down = false;
    return oe;
}

static KeyEntry *lookup_or_create_entry(OrderedExecutor *oe, uint64_t key) {
    size_t idx = key_hash(key, oe->table_size);
    KeyEntry *e = oe->table[idx];
    KeyEntry *prev = NULL;
    while (e) {
        if (e->key == key) return e;
        prev = e;
        e = e->next;
    }
    KeyEntry *ne = calloc(1, sizeof(*ne));
    if (!ne) return NULL;
    ne->key = key;
    ne->head = ne->tail = NULL;
    ne->active = false;
    ne->next = NULL;
    if (prev) prev->next = ne;
    else oe->table[idx] = ne;
    return ne;
}

typedef struct drain_ctx {
    OrderedExecutor *oe;
    uint64_t key;
} drain_ctx_t;

static void drain_worker(void *v) {
    drain_ctx_t *ctx = (drain_ctx_t *)v;
    OrderedExecutor *oe = ctx->oe;
    uint64_t key = ctx->key;
    free(ctx);

    while (1) {
        OETask *task = NULL;
        pthread_mutex_lock(&oe->lock);
        if (oe->shutting_down) {
            KeyEntry *e = lookup_or_create_entry(oe, key); // should exist
            if (e) e->active = false;
            pthread_mutex_unlock(&oe->lock);
            return;
        }

        size_t idx = key_hash(key, oe->table_size);
        KeyEntry *e = oe->table[idx];
        while (e && e->key != key) e = e->next;
        if (!e || !e->head) {
            // no tasks, mark inactive and exit
            if (e) e->active = false;
            pthread_mutex_unlock(&oe->lock);
            return;
        }
        // pop head
        task = e->head;
        e->head = task->next;
        if (!e->head) e->tail = NULL;
        pthread_mutex_unlock(&oe->lock);

        // execute outside lock
        task->fn(task->arg);
        free(task);
    }
}

bool ordered_executor_submit(OrderedExecutor *oe, uint64_t key, OETaskFn fn, void *arg) {
    if (!oe || !fn) return false;
    OETask *t = calloc(1, sizeof(*t));
    if (!t) return false;
    t->fn = fn;
    t->arg = arg;
    t->next = NULL;

    pthread_mutex_lock(&oe->lock);
    if (oe->shutting_down) {
        pthread_mutex_unlock(&oe->lock);
        free(t);
        return false;
    }
    KeyEntry *e = lookup_or_create_entry(oe, key);
    if (!e) {
        pthread_mutex_unlock(&oe->lock);
        free(t);
        return false;
    }
    if (e->tail) e->tail->next = t;
    else e->head = t;
    e->tail = t;

    if (!e->active) {
        e->active = true;
        drain_ctx_t *ctx = malloc(sizeof(*ctx));
        if (!ctx) {
            OETask **pp = &e->head;
            while (*pp && *pp != t) pp = &(*pp)->next;
            if (*pp == t) {
                *pp = t->next;
                if (!e->head) e->tail = NULL;
            }
            pthread_mutex_unlock(&oe->lock);
            free(t);
            return false;
        }
        ctx->oe = oe;
        ctx->key = key;
        bool ok = thread_pool_submit(oe->pool, drain_worker, ctx);
        if (!ok) {
            free(ctx);
            OETask **pp = &e->head;
            while (*pp && *pp != t) pp = &(*pp)->next;
            if (*pp == t) {
                *pp = t->next;
                if (!e->head) e->tail = NULL;
            }
            e->active = false;
            pthread_mutex_unlock(&oe->lock);
            free(t);
            return false;
        }
    }

    pthread_mutex_unlock(&oe->lock);
    return true;
}

void ordered_executor_shutdown(OrderedExecutor *oe) {
    if (!oe) return;
    pthread_mutex_lock(&oe->lock);
    oe->shutting_down = true;
    pthread_mutex_unlock(&oe->lock);
}

void ordered_executor_destroy(OrderedExecutor *oe) {
    if (!oe) return;
    pthread_mutex_lock(&oe->lock);
    for (size_t i = 0; i < oe->table_size; ++i) {
        KeyEntry *e = oe->table[i];
        while (e) {
            KeyEntry *nx = e->next;
            OETask *t = e->head;
            while (t) {
                OETask *tn = t->next;
                free(t);
                t = tn;
            }
            free(e);
            e = nx;
        }
    }
    free(oe->table);
    pthread_mutex_unlock(&oe->lock);

    pthread_mutex_destroy(&oe->lock);
    free(oe);
}

