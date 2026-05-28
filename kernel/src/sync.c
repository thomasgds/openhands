#include "sync.h"
#include "task.h"
#include "kernel_log.h"
#include <stdlib.h>
#include <stdbool.h>

/* 内部阻塞辅助：当前任务直接让出 CPU */
static void block_current(void)
{
    /* 通过 yield 让调度器选择其他任务 */
    /* 实际的阻塞状态由上层同步原语管理 */
    task_yield();
}

void sync_init(void)
{
    LOG_INFO("Sync primitives initialized");
}

/* --- Mutex --- */

mutex_t *mutex_create(const char *name)
{
    mutex_t *m = calloc(1, sizeof(mutex_t));
    if (m) {
        m->name = name;
        m->locked = 0;
        m->owner = NULL;
    }
    return m;
}

void mutex_lock(mutex_t *m)
{
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        /* 锁已被持有，让出 CPU */
        block_current();
    }
    m->owner = (void *)task_self();
}

void mutex_unlock(mutex_t *m)
{
    m->owner = NULL;
    __sync_lock_release(&m->locked);
}

/* --- Semaphore --- */

sem_t *sem_create(const char *name, int initial, int max)
{
    sem_t *s = calloc(1, sizeof(sem_t));
    if (s) {
        s->name = name;
        s->count = initial;
        s->max_count = max;
    }
    return s;
}

void sem_wait(sem_t *s)
{
    while (1) {
        int old = s->count;
        if (old > 0) {
            if (__sync_bool_compare_and_swap(&s->count, old, old - 1))
                return;
        }
        block_current();
    }
}

void sem_signal(sem_t *s)
{
    int old = s->count;
    if (old < s->max_count)
        __sync_fetch_and_add(&s->count, 1);
}

/* --- Message Queue --- */

msg_queue_t *queue_create(const char *name, int capacity)
{
    msg_queue_t *q = calloc(1, sizeof(msg_queue_t));
    if (!q) return NULL;
    q->slots = calloc(capacity, sizeof(void *));
    if (!q->slots) { free(q); return NULL; }
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->name = name;
    return q;
}

int queue_send(msg_queue_t *q, void *msg, uint32_t timeout_ms)
{
    (void)timeout_ms;
    while (q->count >= q->capacity)
        block_current();
    q->slots[q->head] = msg;
    q->head = (q->head + 1) % q->capacity;
    __sync_fetch_and_add(&q->count, 1);
    return 0;
}

int queue_receive(msg_queue_t *q, void **msg, uint32_t timeout_ms)
{
    (void)timeout_ms;
    while (q->count <= 0)
        block_current();
    *msg = q->slots[q->tail];
    q->tail = (q->tail + 1) % q->capacity;
    __sync_fetch_and_sub(&q->count, 1);
    return 0;
}
