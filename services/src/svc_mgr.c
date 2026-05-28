#include "svc_mgr.h"
#include "telnetd.h"
#include "ftpd.h"
#include "sshd.h"
#include "kernel_log.h"
#include "task.h"

void svc_mgr_init(void)
{
    LOG_INFO("Service manager initialized");

    /* 启动网络服务 — 每个服务作为一个独立 RTOS 任务 */
    /* 注意：这些服务会在 TCP 端口上 listen，需要网络可用 */

    task_create("telnetd", (task_func_t)telnetd_start, NULL, 0, TASK_PRIORITY_NORMAL);
    LOG_INFO("  -> Telnet server task created (port 23)");

    task_create("ftpd", (task_func_t)ftpd_start, NULL, 0, TASK_PRIORITY_NORMAL);
    LOG_INFO("  -> FTP server task created (port 21)");

    task_create("sshd", (task_func_t)sshd_start, NULL, 0, TASK_PRIORITY_NORMAL);
    LOG_INFO("  -> SSH server task created (port 22/2222)");

    LOG_INFO("All network services started");
}
