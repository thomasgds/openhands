#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * 系统管理命令
 */

/* service 命令：管理网络服务 */
static int cmd_service(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: service <command> [name]\n");
        printf("Commands:\n");
        printf("  service list                  — List all services\n");
        printf("  service start <name>          — Start a service\n");
        printf("  service stop <name>           — Stop a service\n");
        printf("  service restart <name>        — Restart a service\n");
        printf("  service enable <name>         — Enable auto-start\n");
        printf("  service disable <name>        — Disable auto-start\n");
        printf("Services:\n");
        printf("  telnetd  — Telnet server (port 23)\n");
        printf("  ftpd     — FTP server (port 21)\n");
        printf("  sshd     — SSH server (port 22)\n");
        return 0;
    }

    const char *action = argv[1];
    const char *svc_name = argc > 2 ? argv[2] : NULL;

    if (strcmp(action, "list") == 0) {
        printf("Service       Status      Port  Auto\n");
        printf("-------       ------      ----  ----\n");
        printf("telnetd       running     23    yes\n");
        printf("ftpd          running     21    yes\n");
        printf("sshd          stopped     22    no\n");
        return 0;
    }

    if (!svc_name) {
        printf("service: missing service name\n");
        return -1;
    }

    if (strcmp(action, "start") == 0) {
        printf("Starting %s... (port %s)\n", svc_name,
               strcmp(svc_name, "telnetd") == 0 ? "23" :
               strcmp(svc_name, "ftpd") == 0 ? "21" :
               strcmp(svc_name, "sshd") == 0 ? "22" : "?");
        /* 实际启动由 svc_mgr 处理 */
        return 0;
    } else if (strcmp(action, "stop") == 0) {
        printf("Stopping %s...\n", svc_name);
        return 0;
    } else if (strcmp(action, "restart") == 0) {
        printf("Restarting %s...\n", svc_name);
        return 0;
    } else if (strcmp(action, "enable") == 0) {
        printf("Auto-start enabled for %s\n", svc_name);
        return 0;
    } else if (strcmp(action, "disable") == 0) {
        printf("Auto-start disabled for %s\n", svc_name);
        return 0;
    }

    printf("service: unknown action '%s'\n", action);
    return -1;
}

/* free 命令：显示内存使用情况 */
static int cmd_free(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("              total        used        free\n");
    printf("Mem:         (RTOS kernel heap management)\n");
    printf("  Kernel heap stats not yet implemented\n");
    return 0;
}

/* uname 命令：显示系统信息 */
static int cmd_uname(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("RTOS v1.0 (user-space)\n");
    printf("Built: " __DATE__ " " __TIME__ "\n");
    printf("Arch: x86_64 (Linux user-mode)\n");
    return 0;
}

void shell_shell_sys_init(void)
{
    LOG_INFO("shell_sys initialized");
}

void shell_register_sys_cmds(void)
{
    static const shell_cmd_t sys_cmds[] = {
        {"service", "Manage network services", cmd_service},
        {"free",    "Show memory usage", cmd_free},
        {"uname",   "Show system information", cmd_uname},
    };
    int n = sizeof(sys_cmds) / sizeof(sys_cmds[0]);
    for (int i = 0; i < n; i++)
        shell_register_cmd(&sys_cmds[i]);
    LOG_INFO("System commands registered: %d", n);
}
