#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "kernel.h"
#include "task.h"
#include "sync.h"

static mutex_t *s_print_lock = NULL;

/* ============================================================
 *    Phase 1 演示：多任务并发
 * ============================================================ */
static void worker_task(void *arg)
{
    const char *name = (const char *)arg;
    int count = 0;
    while (count < 3) {
        mutex_lock(s_print_lock);
        printf("[%s] count=%d\n", name, count++);
        mutex_unlock(s_print_lock);
        task_sleep(400 + (rand() % 400));
    }
    mutex_lock(s_print_lock);
    printf("[%s] Done.\n", name);
    mutex_unlock(s_print_lock);
    task_exit();
}

static void demo_scheduler(void *arg)
{
    (void)arg;
    task_sleep(100);
    mutex_lock(s_print_lock);
    printf("\n=== Phase 1: Multi-task Demo ===\n\n");
    mutex_unlock(s_print_lock);

    task_create("WorkerA", worker_task, "WkrA", 0, TASK_PRIORITY_NORMAL);
    task_create("WorkerB", worker_task, "WkrB", 0, TASK_PRIORITY_NORMAL);
    task_create("WorkerC", worker_task, "WkrC", 0, TASK_PRIORITY_LOW);

    mutex_lock(s_print_lock);
    printf("[Demo] 3 worker tasks created\n");
    mutex_unlock(s_print_lock);
    task_exit();
}

/* ============================================================
 *    Phase 2 演示：VFS 设备文件
 * ============================================================ */
#include "vfs.h"

static void demo_vfs(void *arg)
{
    (void)arg;
    task_sleep(3000);
    mutex_lock(s_print_lock);

    printf("\n=== Phase 2: VFS Device Files Demo ===\n");

    int fd = vfs_open("/dev/null", 0);
    printf("[VFS] open /dev/null => fd=%d\n", fd);
    if (fd >= 0) vfs_close(fd);

    fd = vfs_open("/dev/zero", 0);
    printf("[VFS] open /dev/zero => fd=%d\n", fd);
    if (fd >= 0) vfs_close(fd);

    fd = vfs_open("/dev/console", 0);
    printf("[VFS] open /dev/console => fd=%d\n", fd);
    if (fd >= 0) vfs_close(fd);

    fd = vfs_open("/dev/random", 0);
    printf("[VFS] open /dev/random => fd=%d\n", fd);
    if (fd >= 0) vfs_close(fd);

    mutex_unlock(s_print_lock);
    task_exit();
}

/* ============================================================
 *    Phase 3 演示：网络状态
 * ============================================================ */
#include "netif_rtos.h"

static void demo_network(void *arg)
{
    (void)arg;
    task_sleep(5000);
    mutex_lock(s_print_lock);

    printf("\n=== Phase 3: Network Interface ===\n");
    printf("[Net] Status: %s\n", netif_is_up() ? "UP" : "DOWN");
    printf("[Net] IP: %s (configured)\n", netif_get_ip());
    printf("[Net] Note: TUN/TAP requires root or cap_net_admin\n");

    mutex_unlock(s_print_lock);
    task_exit();
}

/* ============================================================
 *    Main Entry
 * ============================================================ */
extern int shell_execute(const char *line);

int main(int argc, char *argv[])
{
    printf("====================================\n");
    printf("  RTOS - Lightweight Real-Time OS\n");
    printf("  Built: " __DATE__ " " __TIME__ "\n");
    printf("====================================\n\n");

    rtos_init();

    s_print_lock = mutex_create("print_lock");

    /* 创建各阶段演示任务 */
    task_create("demo_sched",  demo_scheduler, NULL, 0, TASK_PRIORITY_NORMAL);
    task_create("demo_vfs",    demo_vfs,       NULL, 0, TASK_PRIORITY_LOW);
    task_create("demo_net",    demo_network,   NULL, 0, TASK_PRIORITY_LOW);

    rtos_start();

    /* never reached */
    return 0;
}
