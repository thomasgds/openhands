#include <unistd.h>
#include "task.h"
#include "kernel_log.h"
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_TASKS 32
#define TICK_INTERVAL_MS 10
#define DEFAULT_STACK_SIZE (64 * 1024)

/* 任务节点 — 内含 TCB + ucontext + 栈 */
typedef struct task_node {
    task_t tcb;
    ucontext_t ctx;
    char stack[DEFAULT_STACK_SIZE];
    struct task_node *next;
} task_node_t;

/* 就绪队列 — 每个优先级一个 */
static task_node_t *s_ready_queues[TASK_PRIORITY_COUNT];
static task_node_t *s_current_node = NULL;
static task_t *s_current = NULL;
static int s_next_tid = 1;
static struct sigaction s_old_alarm;
static bool s_scheduler_running = false;

/* 阻塞 / 睡眠队列 */
static task_node_t *s_sleeping_list = NULL;

/* 空闲任务上下文 */
static ucontext_t s_idle_ctx;
static char s_idle_stack[DEFAULT_STACK_SIZE];

/* 主上下文（rtos_start 调用者） */
static ucontext_t s_main_ctx;

/* --- 内部辅助 --- */

static task_node_t *node_from_tcb(task_t *t)
{
    if (!t) return NULL;
    return (task_node_t *)((char *)t - offsetof(task_node_t, tcb));
}

static void enqueue_ready(task_node_t *node)
{
    int prio = node->tcb.priority;
    node->tcb.state = TASK_READY;
    node->next = s_ready_queues[prio];
    s_ready_queues[prio] = node;
}

static task_node_t *dequeue_highest(void)
{
    for (int p = TASK_PRIORITY_COUNT - 1; p >= 0; p--) {
        if (s_ready_queues[p]) {
            task_node_t *n = s_ready_queues[p];
            s_ready_queues[p] = n->next;
            n->tcb.state = TASK_RUNNING;
            return n;
        }
    }
    return NULL;
}

static void enqueue_sleeping(task_node_t *node, uint32_t ticks)
{
    node->tcb.sleep_ticks = ticks;
    node->tcb.state = TASK_SLEEPING;
    task_node_t **pp = &s_sleeping_list;
    while (*pp)
        pp = &(*pp)->next;
    node->next = *pp;
    *pp = node;
}

/* 信号安全的上下文切换 */
static void context_switch_to(task_node_t *next)
{
    if (!next) return;
    task_node_t *prev = s_current_node;
    s_current_node = next;
    s_current = &next->tcb;

    if (prev && prev != next) {
        swapcontext(&prev->ctx, &next->ctx);
    } else {
        setcontext(&next->ctx);
    }
}

/* 调度决策 */
static void schedule(void)
{
    if (!s_scheduler_running) return;
    task_node_t *next = dequeue_highest();
    if (!next) return;

    if (s_current_node && s_current_node->tcb.state == TASK_RUNNING) {
        enqueue_ready(s_current_node);
    }
    context_switch_to(next);
}

void task_scheduler_init(void)
{
    memset(s_ready_queues, 0, sizeof(s_ready_queues));
    LOG_INFO("Task scheduler initialized");
}

static void scheduler_tick_handler(int sig)
{
    (void)sig;
    if (s_scheduler_running)
        task_scheduler_tick();
}

static void idle_task(void *arg)
{
    (void)arg;
    while (1) task_yield();
}

void task_scheduler_start(void)
{
    struct itimerval it;
    struct sigaction sa;

    /* 创建空闲任务 */
    getcontext(&s_idle_ctx);
    s_idle_ctx.uc_stack.ss_sp = s_idle_stack;
    s_idle_ctx.uc_stack.ss_size = sizeof(s_idle_stack);
    s_idle_ctx.uc_link = NULL;
    makecontext(&s_idle_ctx, (void (*)(void))idle_task, 1, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = scheduler_tick_handler;
    sa.sa_flags = SA_RESTART | SA_NODEFER;
    sigaction(SIGALRM, &sa, &s_old_alarm);

    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = TICK_INTERVAL_MS * 1000;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = TICK_INTERVAL_MS * 1000;
    setitimer(ITIMER_REAL, &it, NULL);

    s_scheduler_running = true;
    LOG_INFO("Scheduler starting with %d ms tick", TICK_INTERVAL_MS);

    /* 运行第一个任务 */
    task_node_t *first = dequeue_highest();
    if (first) {
        s_current_node = first;
        s_current = &first->tcb;
        swapcontext(&s_main_ctx, &first->ctx);
    } else {
        swapcontext(&s_main_ctx, &s_idle_ctx);
    }
}

void task_scheduler_tick(void)
{
    if (!s_scheduler_running) return;
    if (s_current_node)
        s_current_node->tcb.time_ticks++;

    /* 唤醒睡眠任务 */
    task_node_t **pp = &s_sleeping_list;
    while (*pp) {
        if ((*pp)->tcb.sleep_ticks > 0)
            (*pp)->tcb.sleep_ticks--;
        if ((*pp)->tcb.sleep_ticks == 0) {
            task_node_t *wake = *pp;
            *pp = wake->next;
            enqueue_ready(wake);
        } else {
            pp = &(*pp)->next;
        }
    }

    /* 抢占式：相同优先级时间片轮转 */
    if (s_current_node && s_current_node->tcb.state == TASK_RUNNING) {
        enqueue_ready(s_current_node);
        task_node_t *next = dequeue_highest();
        if (next && next != s_current_node) {
            context_switch_to(next);
        } else if (next) {
            /* 没有其他任务，放回去继续运行 */
            enqueue_ready(s_current_node);
        }
    }
}

/* --- 公开 API --- */

task_t *task_create(const char *name, task_func_t func, void *arg,
                    size_t stack_size, int priority)
{
    if (priority < 0 || priority >= TASK_PRIORITY_COUNT)
        priority = TASK_PRIORITY_NORMAL;
    if (stack_size < 1024) stack_size = DEFAULT_STACK_SIZE;

    task_node_t *node = calloc(1, sizeof(task_node_t));
    if (!node) return NULL;

    strncpy(node->tcb.name, name ? name : "task", sizeof(node->tcb.name) - 1);
    node->tcb.tid = s_next_tid++;
    node->tcb.priority = priority;
    node->tcb.base_priority = priority;
    node->tcb.state = TASK_READY;
    node->tcb.stack_size = stack_size;

    /* 设置 ucontext */
    getcontext(&node->ctx);
    node->ctx.uc_stack.ss_sp = node->stack;
    node->ctx.uc_stack.ss_size = sizeof(node->stack);
    node->ctx.uc_link = &s_idle_ctx;
    makecontext(&node->ctx, (void (*)(void))func, 1, arg);

    enqueue_ready(node);

    LOG_INFO("task_create: %s (tid=%d, prio=%d)",
             node->tcb.name, node->tcb.tid, priority);
    return &node->tcb;
}

void task_destroy(task_t *task)
{
    if (!task) return;
    node_from_tcb(task)->tcb.state = TASK_TERMINATED;
    free(node_from_tcb(task));
}

void task_yield(void)
{
    if (!s_scheduler_running || !s_current_node) return;
    schedule();
}

void task_sleep(uint32_t ms)
{
    if (!s_current_node) return;
    uint32_t ticks = (ms + TICK_INTERVAL_MS - 1) / TICK_INTERVAL_MS;
    if (ticks == 0) ticks = 1;
    enqueue_sleeping(s_current_node, ticks);
    s_current_node = NULL;
    s_current = NULL;
    schedule();
}

void task_exit(void)
{
    if (s_current_node) {
        s_current_node->tcb.state = TASK_TERMINATED;
        s_current_node = NULL;
        s_current = NULL;
    }
    schedule();
}

task_t *task_self(void)
{
    return s_current;
}

int task_get_tid(task_t *t) { return t ? t->tid : -1; }
const char *task_get_name(task_t *t) { return t ? t->name : "none"; }
void task_set_priority(task_t *t, int priority) { if (t) t->priority = priority; }
