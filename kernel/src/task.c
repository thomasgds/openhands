#include <unistd.h>
#include "task.h"
#include "kernel_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sched.h>

#define MAX_TASKS 32
#define TICK_INTERVAL_MS 10

/* 任务控制块 — 每个任务一个 POSIX 线程 */
typedef struct task_node {
    task_t tcb;
    pthread_t thread;
    bool created;
    bool running;
    struct task_node *next;
} task_node_t;

static task_node_t s_tasks[MAX_TASKS];
static int s_task_count = 0;
static task_t *s_current = NULL;
static pthread_key_t s_task_key;

static bool s_scheduler_running = false;

/* Tick 相关 */
static struct sigaction s_old_alarm;
static volatile uint32_t s_tick_count = 0;

/* --- 内部辅助 --- */

static task_node_t *find_node_by_thread(pthread_t thread)
{
    for (int i = 0; i < s_task_count; i++)
        if (s_tasks[i].created && s_tasks[i].thread == thread)
            return &s_tasks[i];
    return NULL;
}

static void *task_wrapper(void *arg)
{
    task_node_t *node = (task_node_t *)arg;
    task_func_t func = (task_func_t)node->tcb.entry;
    void *func_arg = node->tcb.arg;

    /* 设置 TLS 指向当前 TCB */
    pthread_setspecific(s_task_key, &node->tcb);
    s_current = &node->tcb;

    /* 调用用户任务函数 */
    func(func_arg);

    node->running = false;
    node->tcb.state = TASK_TERMINATED;
    return NULL;
}

/* --- 调度器管理 --- */

void task_scheduler_init(void)
{
    pthread_key_create(&s_task_key, NULL);
    memset(s_tasks, 0, sizeof(s_tasks));
    LOG_INFO("Task scheduler initialized (pthread-based)");
}

static void scheduler_tick_handler(int sig)
{
    (void)sig;
    s_tick_count++;
}

void task_scheduler_start(void)
{
    struct itimerval it;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = scheduler_tick_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, &s_old_alarm);

    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = TICK_INTERVAL_MS * 1000;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = TICK_INTERVAL_MS * 1000;
    setitimer(ITIMER_REAL, &it, NULL);

    s_scheduler_running = true;
    LOG_INFO("Scheduler starting with %d ms tick", TICK_INTERVAL_MS);
}

void task_scheduler_tick(void)
{
    /* 统计 tick — 实际调度由 pthread 调度器处理 */
}

/* --- 公开 API --- */

task_t *task_create(const char *name, task_func_t func, void *arg,
                    size_t stack_size, int priority)
{
    if (s_task_count >= MAX_TASKS) return NULL;
    if (priority < 0 || priority >= TASK_PRIORITY_COUNT)
        priority = TASK_PRIORITY_NORMAL;
    if (stack_size < 4096) stack_size = 64 * 1024;

    task_node_t *node = &s_tasks[s_task_count];
    memset(node, 0, sizeof(*node));

    strncpy(node->tcb.name, name ? name : "task", sizeof(node->tcb.name) - 1);
    node->tcb.priority = priority;
    node->tcb.base_priority = priority;
    node->tcb.state = TASK_RUNNING;
    node->tcb.entry = (void *)func;
    node->tcb.arg = arg;
    node->tcb.tid = s_task_count + 1;
    node->running = true;
    node->created = true;

    /* 创建 pthread */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stack_size);

    if (priority > TASK_PRIORITY_NORMAL) {
        struct sched_param param;
        param.sched_priority = 10 + priority;
        pthread_attr_setschedpolicy(&attr, SCHED_RR);
        pthread_attr_setschedparam(&attr, &param);
    }

    int ret = pthread_create(&node->thread, &attr, task_wrapper, node);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        LOG_ERROR("task_create: pthread_create failed for %s (err=%d)", name, ret);
        node->created = false;
        return NULL;
    }

    s_task_count++;
    LOG_INFO("task_create: %s (tid=%d, prio=%d)", name, node->tcb.tid, priority);
    return &node->tcb;
}

void task_destroy(task_t *task)
{
    if (!task) return;
    /* 实际清理在线程退出时自动完成 */
}

void task_yield(void)
{
    /* pthread 版本的 yield 让出 CPU */
    sched_yield();
}

void task_sleep(uint32_t ms)
{
    usleep(ms * 1000);
}

void task_exit(void)
{
    task_node_t *node = find_node_by_thread(pthread_self());
    if (node) {
        node->running = false;
        node->tcb.state = TASK_TERMINATED;
    }
    pthread_exit(NULL);
}

task_t *task_self(void)
{
    task_t *t = (task_t *)pthread_getspecific(s_task_key);
    if (!t) {
        /* 主线程调用 */
        return NULL;
    }
    return t;
}

int task_get_tid(task_t *t) { return t ? t->tid : 0; }
const char *task_get_name(task_t *t) { return t ? t->name : "main"; }
void task_set_priority(task_t *t, int priority) { if (t) t->priority = priority; }

/* 等待所有任务结束 */
void task_join_all(void)
{
    for (int i = 0; i < s_task_count; i++) {
        if (s_tasks[i].created)
            pthread_join(s_tasks[i].thread, NULL);
    }
}
