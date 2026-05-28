#include "shell.h"
#include "kernel_log.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* 外部命令 */
extern int net_cmd_ping(const char *host);
extern void net_cmd_ifconfig(void);

/* --- 内置命令实现 --- */

static int cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    /* help is handled directly in shell_execute */
    printf("This is handled automatically.\n");
    return 0;
}

static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        printf("%s%s", i > 1 ? " " : "", argv[i]);
    }
    printf("\n");
    return 0;
}

static int cmd_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("\033[2J\033[H");
    return 0;
}

static int cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    time_t t = time(NULL);
    printf("System time: %s", ctime(&t));
    printf("Tasks: running\n");
    return 0;
}

static int cmd_ps(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("PID  NAME           PRIO STATE\n");
    printf("---  ----           ---- -----\n");
    printf("  -  (task list not fully tracked)\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Rebooting... (not implemented)\n");
    return 0;
}

static int cmd_ifconfig(int argc, char **argv)
{
    (void)argc; (void)argv;
    net_cmd_ifconfig();
    return 0;
}

static int cmd_ping(int argc, char **argv)
{
    const char *host = (argc > 1) ? argv[1] : NULL;
    net_cmd_ping(host);
    return 0;
}

static int cmd_date(int argc, char **argv)
{
    (void)argc; (void)argv;
    time_t t = time(NULL);
    printf("%s", ctime(&t));
    return 0;
}

/* 命令注册表 */
static const shell_cmd_t s_builtin_cmds[] = {
    {"echo",     "Echo text to console",        cmd_echo},
    {"clear",    "Clear the screen",             cmd_clear},
    {"uptime",   "Show system uptime",           cmd_uptime},
    {"ps",       "List running tasks",           cmd_ps},
    {"reboot",   "Reboot the system",            cmd_reboot},
    {"ifconfig", "Show network configuration",   cmd_ifconfig},
    {"ping",     "Ping a network host",          cmd_ping},
    {"date",     "Show current date and time",   cmd_date},
};

void shell_register_all_commands(void)
{
    int n = sizeof(s_builtin_cmds) / sizeof(s_builtin_cmds[0]);
    for (int i = 0; i < n; i++)
        shell_register_cmd(&s_builtin_cmds[i]);
    LOG_INFO("Commands registered: %d built-in", n);
}
