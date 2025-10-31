#define _GNU_SOURCE
#include "p2pipe/threads.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct task {
    tp_task_fn fn;
    void *arg;
    struct task *next;
} task_t;

struct thread_pool {
    pthread_t *threads;
    size_t n_threads;

    pthread_mutex_t lock;
    pthread_cond_t cond;
    task_t *head;
    task_t *tail;
    size_t task_count;

    bool stopping;
    bool stopped;
};

static void *worker_main(void *vpool) {
    thread_pool_t *p = (thread_pool_t *)vpool;

    while (1) {
        pthread_mutex_lock(&p->lock);
        while (!p->stopping && p->task_count == 0) {
            pthread_cond_wait(&p->cond, &p->lock);
        }
        if (p->stopping && p->task_count == 0) {
            pthread_mutex_unlock(&p->lock);
            break;
        }

        task_t *t = p->head;
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

thread_pool_t *thread_pool_create(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;
    thread_pool_t *p = calloc(1, sizeof(*p));
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

bool thread_pool_submit(thread_pool_t *p, tp_task_fn fn, void *arg) {
    if (!p || !fn) return false;
    task_t *t = malloc(sizeof(*t));
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

void thread_pool_shutdown(thread_pool_t *p) {
    if (!p) return;
    pthread_mutex_lock(&p->lock);
    p->stopping = true;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);

    for (size_t i = 0; i < p->n_threads; ++i) {
        pthread_join(p->threads[i], NULL);
    }

    pthread_mutex_lock(&p->lock);
    task_t *t = p->head;
    while (t) {
        task_t *n = t->next;
        free(t);
        t = n;
    }
    p->head = p->tail = NULL;
    p->task_count = 0;
    pthread_mutex_unlock(&p->lock);

    p->stopped = true;
}

void thread_pool_destroy(thread_pool_t *p) {
    if (!p) return;
    thread_pool_shutdown(p);
    free(p->threads);
    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->cond);
    free(p);
}

typedef struct oe_task {
    oe_task_fn fn;
    void *arg;
    struct oe_task *next;
} oe_task_t;

typedef struct key_entry {
    uint64_t key;
    oe_task_t *head;
    oe_task_t *tail;
    bool active;
    struct key_entry *next;
} key_entry_t;

struct ordered_executor {
    thread_pool_t *pool;

    pthread_mutex_t lock;
    size_t table_size;
    key_entry_t **table;

    bool shutting_down;
};

static inline size_t key_hash(uint64_t key, size_t table_size) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    return (size_t)(key) & (table_size - 1);
}

ordered_executor_t *ordered_executor_create(thread_pool_t *pool, size_t capacity_hint) {
    ordered_executor_t *oe = calloc(1, sizeof(*oe));
    if (!oe) return NULL;
    oe->pool = pool;
    pthread_mutex_init(&oe->lock, NULL);
    size_t ts = 1;
    while (ts < capacity_hint) ts <<= 1;
    if (ts < 16) ts = 16;
    oe->table_size = ts;
    oe->table = calloc(ts, sizeof(key_entry_t*));
    if (!oe->table) { free(oe); return NULL; }
    oe->shutting_down = false;
    return oe;
}

static key_entry_t *lookup_or_create_entry(ordered_executor_t *oe, uint64_t key) {
    size_t idx = key_hash(key, oe->table_size);
    key_entry_t *e = oe->table[idx];
    key_entry_t *prev = NULL;
    while (e) {
        if (e->key == key) return e;
        prev = e;
        e = e->next;
    }
    key_entry_t *ne = calloc(1, sizeof(*ne));
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
    ordered_executor_t *oe;
    uint64_t key;
} drain_ctx_t;

static void drain_worker(void *v) {
    drain_ctx_t *ctx = (drain_ctx_t *)v;
    ordered_executor_t *oe = ctx->oe;
    uint64_t key = ctx->key;
    free(ctx);

    while (1) {
        oe_task_t *task = NULL;
        pthread_mutex_lock(&oe->lock);
        if (oe->shutting_down) {
            key_entry_t *e = lookup_or_create_entry(oe, key); // should exist
            if (e) e->active = false;
            pthread_mutex_unlock(&oe->lock);
            return;
        }

        size_t idx = key_hash(key, oe->table_size);
        key_entry_t *e = oe->table[idx];
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

bool ordered_executor_submit(ordered_executor_t *oe, uint64_t key, oe_task_fn fn, void *arg) {
    if (!oe || !fn) return false;
    oe_task_t *t = calloc(1, sizeof(*t));
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
    key_entry_t *e = lookup_or_create_entry(oe, key);
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
            oe_task_t **pp = &e->head;
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
            oe_task_t **pp = &e->head;
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

void ordered_executor_shutdown(ordered_executor_t *oe) {
    if (!oe) return;
    pthread_mutex_lock(&oe->lock);
    oe->shutting_down = true;
    pthread_mutex_unlock(&oe->lock);
}

void ordered_executor_destroy(ordered_executor_t *oe) {
    if (!oe) return;
    pthread_mutex_lock(&oe->lock);
    for (size_t i = 0; i < oe->table_size; ++i) {
        key_entry_t *e = oe->table[i];
        while (e) {
            key_entry_t *nx = e->next;
            oe_task_t *t = e->head;
            while (t) {
                oe_task_t *tn = t->next;
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

