#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mutex {
    volatile int locked;
    const char *name;
    void *owner;
} mutex_t;

typedef struct semaphore {
    volatile int count;
    int max_count;
    const char *name;
} sem_t;

typedef struct msg_queue {
    void **slots;
    int capacity;
    int head;
    int tail;
    int count;
    const char *name;
} msg_queue_t;

mutex_t *mutex_create(const char *name);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

sem_t *sem_create(const char *name, int initial, int max);
void sem_wait(sem_t *s);
void sem_signal(sem_t *s);

msg_queue_t *queue_create(const char *name, int capacity);
int queue_send(msg_queue_t *q, void *msg, uint32_t timeout_ms);
int queue_receive(msg_queue_t *q, void **msg, uint32_t timeout_ms);

void sync_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNC_H */
