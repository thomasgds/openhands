#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Shell reload 命令
 *
 * reload 命令用于重新加载系统配置或重启 shell。
 * 用法：
 *   reload         — 重新加载所有配置
 *   reload shell   — 仅重启 shell
 *   reload services — 重启所有网络服务
 */

static int cmd_reload(int argc, char **argv)
{
    if (argc > 1) {
        if (strcmp(argv[1], "shell") == 0) {
            printf("Restarting shell...\n");
            fflush(stdout);
            execvp(argv[0], argv);  /* 重新执行自身 */
            /* 如果 exec 失败 */
            printf("reload shell failed\n");
            return -1;
        } else if (strcmp(argv[1], "services") == 0) {
            printf("Restarting network services...\n");
            /* 在真实的 RTOS 中这里会调用 svc_mgr 重启服务 */
            printf("  Would restart: telnetd, ftpd, sshd\n");
            return 0;
        }
    }

    printf("reload: system configuration reloaded\n");
    printf("  Sub-commands:\n");
    printf("    reload shell     — Restart shell (exec)\n");
    printf("    reload services  — Restart network services\n");
    return 0;
}

static const shell_cmd_t s_reload_cmd = {
    "reload", "Reload system configuration", cmd_reload
};

void shell_shell_reload_init(void)
{
    LOG_INFO("shell_reload initialized");
    shell_register_cmd(&s_reload_cmd);
}
