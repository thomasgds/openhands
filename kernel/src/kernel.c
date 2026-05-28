#include "kernel.h"
#include "task.h"
#include "sync.h"
#include "memory.h"
#include "timer.h"
#include "hal.h"
#include "vfs.h"
#include "devfs.h"
#include "netif_rtos.h"
#include "shell.h"
#include "svc_mgr.h"
#include <stdbool.h>

static bool s_kernel_running = false;

void rtos_init(void)
{
    LOG_INFO("RTOS initializing...");

    /* HAL layer */
    hal_init();

    /* Phase 1: Kernel core */
    memory_init();
    task_scheduler_init();
    sync_init();
    timer_init();

    /* Phase 2: VFS */
    vfs_init();
    devfs_init();

    /* Phase 3: Network */
    lwip_init_rtos();

    LOG_INFO("RTOS initialization complete.");
}

void rtos_start(void)
{
    LOG_INFO("RTOS starting...");
    s_kernel_running = true;

    /* Start network thread */
    lwip_thread_start();

    /* Mount filesystem */
    vfs_mount_disk();

    /* Start Shell */
    shell_init();

    /* Start network services */
    svc_mgr_init();

    /* Start scheduler timer */
    task_scheduler_start();

    LOG_INFO("RTOS is running. Waiting for tasks to complete...");

    /* 等待所有任务结束 */
    task_join_all();

    s_kernel_running = false;
    LOG_INFO("RTOS stopped.");
}

bool kernel_is_running(void)
{
    return s_kernel_running;
}
