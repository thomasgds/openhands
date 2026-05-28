#include "shell.h"
#include "kernel_log.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_CMDS 64
#define MAX_ARGS 32
#define MAX_LINE 512

static const shell_cmd_t *s_cmds[MAX_CMDS];
static int s_num_cmds = 0;
static bool s_shell_exit = false;

extern void shell_register_all_commands(void);
extern void shell_register_net_cmds(void);
extern void shell_register_sys_cmds(void);
extern void shell_register_fs_cmds(void);

void shell_register_cmd(const shell_cmd_t *cmd)
{
    if (cmd && s_num_cmds < MAX_CMDS)
        s_cmds[s_num_cmds++] = cmd;
}

static const shell_cmd_t *find_cmd(const char *name)
{
    for (int i = 0; i < s_num_cmds; i++) {
        if (strcmp(s_cmds[i]->name, name) == 0)
            return s_cmds[i];
    }
    return NULL;
}

static void print_help(void)
{
    printf("Available commands:\n");
    for (int i = 0; i < s_num_cmds; i++) {
        printf("  %-12s %s\n", s_cmds[i]->name, s_cmds[i]->help);
    }
}

/* 简单的行解析 — 按空格分词，支持双引号字符串 */
static int parse_line(const char *line, char **argv, int max_args)
{
    int argc = 0;
    const char *p = line;

    while (*p && argc < max_args) {
        /* 跳过空白 */
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        if (*p == '"') {
            /* 双引号字符串 */
            p++;
            argv[argc] = (char *)p;
            while (*p && *p != '"') p++;
            if (*p) {
                *(char *)p = '\0';
                p++;
            }
        } else {
            argv[argc] = (char *)p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) {
                *(char *)p = '\0';
                p++;
            }
        }
        argc++;
    }

    return argc;
}

int shell_execute(const char *line)
{
    if (!line || !*line) return 0;

    char *argv[MAX_ARGS];
    int argc = parse_line(line, argv, MAX_ARGS);
    if (argc == 0) return 0;

    /* help 命令特殊处理 */
    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
        print_help();
        return 0;
    }

    /* exit 命令 */
    if (strcmp(argv[0], "exit") == 0) {
        s_shell_exit = true;
        return 0;
    }

    const shell_cmd_t *cmd = find_cmd(argv[0]);
    if (!cmd) {
        printf("Unknown command: %s (type 'help' for list)\n", argv[0]);
        return -1;
    }

    return cmd->func(argc, argv);
}

static void shell_task(void *arg)
{
    (void)arg;
    char line[MAX_LINE];

    printf("====================================\n");
    printf("  RTOS Shell v1.0\n");
    printf("  Type 'help' for commands\n");
    printf("====================================\n");

    while (!s_shell_exit) {
        printf("rtos> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        shell_execute(line);
    }

    printf("\nShell exiting.\n");
    task_exit();
}

void shell_init(void)
{
    shell_register_all_commands();
    shell_register_net_cmds();
    shell_register_sys_cmds();
    shell_register_fs_cmds();
    task_create("shell", shell_task, NULL, 0, TASK_PRIORITY_NORMAL);
    LOG_INFO("Shell initialized (%d commands)", s_num_cmds);
}
