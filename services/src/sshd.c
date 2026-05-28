#include "sshd.h"
#include "kernel_log.h"
#include "task.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define SSH_PORT 22
#define SSH_BACKLOG 5

static int s_sshd_fd = -1;
static bool s_sshd_running = false;

/*
 * SSH 服务端实现
 *
 * 方案：
 *   监听端口 22，接受连接后 fork 子进程，
 *   将 socket fd dup2 到 stdin/stdout/stderr，
 *   然后 exec /usr/sbin/sshd -i（inetd 模式）
 *
 * 后备方案：
 *   如果 sshd 不可用，使用 system() 启动 dropbear。
 *   如果都不存在，打印提示让用户安装。
 */

static bool has_sshd(void)
{
    static int checked = 0;
    static int exists = 0;
    if (!checked) {
        checked = 1;
        exists = (access("/usr/sbin/sshd", X_OK) == 0) ||
                 (access("/usr/sbin/dropbear", X_OK) == 0);
    }
    return !!exists;
}

static const char *get_sshd_path(void)
{
    if (access("/usr/sbin/sshd", X_OK) == 0)
        return "/usr/sbin/sshd";
    if (access("/usr/sbin/dropbear", X_OK) == 0)
        return "/usr/sbin/dropbear";
    return NULL;
}

static int get_sshd_port(void)
{
    /* 如果端口 22 已被占用（比如系统 sshd 已在运行），用 2222 */
    struct sockaddr_in test;
    memset(&test, 0, sizeof(test));
    test.sin_family = AF_INET;
    test.sin_port = htons(SSH_PORT);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return SSH_PORT;
    int rc = bind(fd, (struct sockaddr *)&test, sizeof(test));
    close(fd);
    return (rc == 0) ? SSH_PORT : 2222;
}

static void ssh_client_fork(void *arg)
{
    int client_fd = (int)(intptr_t)arg;

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("SSH fork failed");
        close(client_fd);
        task_exit();
        return;
    }

    if (pid == 0) {
        /* 子进程 */
        dup2(client_fd, STDIN_FILENO);
        dup2(client_fd, STDOUT_FILENO);
        dup2(client_fd, STDERR_FILENO);
        close(client_fd);

        const char *sshd_path = get_sshd_path();
        if (!sshd_path) {
            dprintf(STDERR_FILENO, "No SSH server found.\r\n");
            _exit(1);
        }

        /* 启动 sshd -i 或 dropbear -i */
        if (strstr(sshd_path, "sshd")) {
            execl(sshd_path, "sshd", "-i", NULL);
        } else if (strstr(sshd_path, "dropbear")) {
            /* dropbear -E -F 表示前台运行，-p none 表示用标准 I/O */
            execl(sshd_path, "dropbear", "-i", "-E", NULL);
        }
        dprintf(STDERR_FILENO, "exec %s failed\r\n", sshd_path);
        _exit(1);
    }

    /* 父进程 */
    close(client_fd);
    waitpid(pid, NULL, 0);  /* 等待子进程结束 */
    LOG_INFO("SSH session ended (pid=%d)", pid);
    task_exit();
}

void sshd_start(void)
{
    if (s_sshd_fd >= 0) {
        LOG_WARN("SSH server already running");
        return;
    }

    int port = get_sshd_port();
    const char *sshd_path = get_sshd_path();

    if (!sshd_path) {
        LOG_WARN("SSH server: no sshd or dropbear found on system");
        LOG_WARN("  Install: sudo apt-get install openssh-server");
        LOG_WARN("  Or:      sudo apt-get install dropbear");
        return;
    }

    s_sshd_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_sshd_fd < 0) {
        LOG_ERROR("SSH socket creation failed");
        return;
    }

    int opt = 1;
    setsockopt(s_sshd_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s_sshd_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("SSH bind port %d failed", port);
        close(s_sshd_fd);
        s_sshd_fd = -1;
        return;
    }

    listen(s_sshd_fd, SSH_BACKLOG);
    s_sshd_running = true;
    LOG_INFO("SSH server listening on port %d (via %s)", port, sshd_path);

    while (s_sshd_running) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int client_fd = accept(s_sshd_fd, (struct sockaddr *)&client, &client_len);
        if (client_fd < 0) break;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
        LOG_INFO("SSH connection from %s:%d", ip, ntohs(client.sin_port));

        task_create("ssh_client", ssh_client_fork,
                    (void *)(intptr_t)client_fd, 0, TASK_PRIORITY_NORMAL);
    }

    /* 清理 */
    close(s_sshd_fd);
    s_sshd_fd = -1;
    s_sshd_running = false;
}

void sshd_stop(void)
{
    s_sshd_running = false;
    if (s_sshd_fd >= 0) {
        close(s_sshd_fd);
        s_sshd_fd = -1;
    }
    LOG_INFO("SSH server stopped");
}
