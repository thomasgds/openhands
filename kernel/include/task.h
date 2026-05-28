#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 任务优先级 */
#define TASK_PRIORITY_IDLE      0
#define TASK_PRIORITY_LOW       1
#define TASK_PRIORITY_NORMAL    2
#define TASK_PRIORITY_HIGH      3
#define TASK_PRIORITY_REALTIME  4
#define TASK_PRIORITY_COUNT     5

/* 任务状态 */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_TERMINATED
} task_state_t;

/* 任务控制块 */
typedef struct task_tcb {
    char name[32];
    task_state_t state;
    int priority;
    int base_priority;
    void *stack_ptr;
    size_t stack_size;
    void *entry;
    void *arg;
    int tid;
    uint32_t time_ticks;
    uint32_t sleep_ticks;
    struct task_tcb *next;
    struct task_tcb *prev;
    void *wait_queue;
} task_t;

typedef void (*task_func_t)(void *arg);

task_t *task_create(const char *name, task_func_t func, void *arg,
                    size_t stack_size, int priority);
void task_destroy(task_t *task);
void task_yield(void);
void task_sleep(uint32_t ms);
void task_exit(void);
task_t *task_self(void);

void task_scheduler_init(void);
void task_scheduler_start(void);
void task_scheduler_tick(void);
int task_get_tid(task_t *t);
const char *task_get_name(task_t *t);
void task_set_priority(task_t *t, int priority);

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */
