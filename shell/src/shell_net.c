#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h>

/* ============================================================
 *   SSH 客户端 — 用 system() 调用系统的 ssh 命令
 * ============================================================ */

static int ssh_connect(const char *host, int port, const char *user,
                       const char *pass, const char *cmd)
{
    /* 最简单可靠的方式：生成 sshpass + ssh 命令，exec 到子进程
       这样 SSH 客户端本身处理终端 I/O 和所有交互细节 */
    char buf[4096];

    if (cmd) {
        snprintf(buf, sizeof(buf),
                 "sshpass -p '%s' ssh -o StrictHostKeyChecking=no "
                 "-o UserKnownHostsFile=/dev/null "
                 "-p %d %s@%s '%s'",
                 pass, port, user, host, cmd);
    } else {
        snprintf(buf, sizeof(buf),
                 "sshpass -p '%s' ssh -o StrictHostKeyChecking=no "
                 "-o UserKnownHostsFile=/dev/null "
                 "-p %d %s@%s",
                 pass, port, user, host);
    }

    /* 直接用 system() 运行，会阻塞直到 ssh 退出 */
    int ret = system(buf);

    /* 如果 sshpass 不存在，回退到直接 ssh（交互式输密码） */
    if (ret == -1 || WEXITSTATUS(ret) == 127) {
        printf("ssh: 'sshpass' not found, falling back to interactive ssh...\n");
        if (cmd) {
            snprintf(buf, sizeof(buf),
                     "ssh -o StrictHostKeyChecking=no "
                     "-o UserKnownHostsFile=/dev/null "
                     "-p %d %s@%s '%s'",
                     port, user, host, cmd);
        } else {
            snprintf(buf, sizeof(buf),
                     "ssh -o StrictHostKeyChecking=no "
                     "-o UserKnownHostsFile=/dev/null "
                     "-p %d %s@%s",
                     port, user, host);
        }
        ret = system(buf);
    }

    return ret;
}

/* Shell 命令: ssh <user>@<host>[:port] <command> */
static int cmd_ssh(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: ssh <user@host[:port]> <command>\n");
        printf("  or:  ssh <user@host[:port]>\n");
        printf("Examples:\n");
        printf("  ssh root@192.168.1.1 'ls -la'\n");
        printf("  ssh admin@server:2222 'df -h'\n");
        return -1;
    }

    char user[128] = "root";
    char host[256];
    int port = 22;
    const char *cmd = NULL;

    /* 解析 user@host[:port] */
    char *at = strchr(argv[1], '@');
    if (at) {
        size_t ulen = at - argv[1];
        if (ulen > 0 && ulen < sizeof(user)) {
            strncpy(user, argv[1], ulen);
            user[ulen] = '\0';
        }
        char *hostpart = at + 1;
        char *colon = strchr(hostpart, ':');
        if (colon) {
            size_t hlen = colon - hostpart;
            if (hlen < sizeof(host)) {
                strncpy(host, hostpart, hlen);
                host[hlen] = '\0';
            }
            port = atoi(colon + 1);
            if (port <= 0) port = 22;
        } else {
            strncpy(host, hostpart, sizeof(host) - 1);
        }
    } else {
        strncpy(host, argv[1], sizeof(host) - 1);
    }

    if (argc > 2) {
        cmd = argv[2];
    }

    /* 密码：优先环境变量 SSH_PASS */
    char pass[256] = "";
    char *env_pass = getenv("SSH_PASS");
    if (env_pass && env_pass[0]) {
        strncpy(pass, env_pass, sizeof(pass) - 1);
    }

    printf("Connecting to %s@%s:%d ...\n", user, host, port);
    if (cmd) printf("Command: %s\n", cmd);

    return ssh_connect(host, port, user, pass, cmd);
}

/* Shell 命令: scp — 按 SSH 连接方式复用 */
static int cmd_scp(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: scp <user@host:/path/to/file> <local_dest>\n");
        printf("  or:  scp <local_file> <user@host:/path/to/dest>\n");
        printf("Note: scp requires remote execution, use FTP instead for file transfer\n");
        return -1;
    }
    printf("scp: not yet implemented as interactive. Use FTP client instead.\n");
    printf("  ftp> put <local> <remote>\n");
    printf("  ftp> get <remote> <local>\n");
    return 0;
}

void shell_shell_net_init(void) { LOG_INFO("shell_net initialized"); }

/* 直接调用来注册 */
void shell_register_net_cmds(void)
{
    static const shell_cmd_t net_cmds[] = {
        {"ssh", "SSH client: ssh <user@host> <command>", cmd_ssh},
        {"scp", "SCP file transfer (use FTP instead)", cmd_scp},
    };
    int n = sizeof(net_cmds) / sizeof(net_cmds[0]);
    for (int i = 0; i < n; i++)
        shell_register_cmd(&net_cmds[i]);
    LOG_INFO("Network commands registered: %d", n);
}
