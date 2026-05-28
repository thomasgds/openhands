#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <libssh2.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

/* ============================================================
 *   SSH 客户端 — 使用 libssh2
 * ============================================================ */

static int ssh_connect(const char *host, int port, const char *user,
                       const char *pass, const char *cmd)
{
    /* 解析主机名 */
    struct hostent *he = gethostbyname(host);
    if (!he) {
        printf("ssh: unknown host %s\n", host);
        return -1;
    }

    /* 建立 TCP 连接 */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("ssh: socket error\n");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("ssh: connect to %s:%d failed (%s)\n", host, port, strerror(errno));
        close(sock);
        return -1;
    }

    /* 初始化 libssh2 会话 */
    LIBSSH2_SESSION *session = libssh2_session_init();
    if (!session) {
        printf("ssh: session init failed\n");
        close(sock);
        return -1;
    }

    libssh2_session_set_blocking(session, 1);

    /* 握手 */
    int rc = libssh2_session_handshake(session, sock);
    if (rc) {
        printf("ssh: handshake failed (code=%d)\n", rc);
        libssh2_session_free(session);
        close(sock);
        return -1;
    }

    /* 密码认证 */
    rc = libssh2_userauth_password(session, user, pass);
    if (rc) {
        printf("ssh: authentication failed (code=%d)\n", rc);
        char *err_msg = NULL;
        libssh2_session_last_error(session, &err_msg, NULL, 0);
        if (err_msg) printf("  Error: %s\n", err_msg);
        libssh2_session_free(session);
        close(sock);
        return -1;
    }

    printf("ssh: connected to %s as %s\n", host, user);

    if (cmd) {
        /* 单命令模式：打开通道 -> exec -> 读输出 -> 退出 */
        LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
        if (!channel) {
            printf("ssh: channel open failed\n");
            libssh2_session_free(session);
            close(sock);
            return -1;
        }

        rc = libssh2_channel_exec(channel, cmd);
        if (rc) {
            printf("ssh: exec failed (code=%d)\n", rc);
            libssh2_channel_close(channel);
            libssh2_channel_free(channel);
            libssh2_session_free(session);
            close(sock);
            return -1;
        }

        /* 读取输出 */
        char buf[4096];
        while (1) {
            int n = libssh2_channel_read(channel, buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = '\0';
            printf("%s", buf);
        }

        libssh2_channel_send_eof(channel);
        libssh2_channel_wait_eof(channel);
        libssh2_channel_close(channel);
        libssh2_channel_free(channel);
    } else {
        /* 交互式 shell 模式：打开 channel shell -> stdin/stdout 双向转发 */
        LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
        if (!channel) {
            printf("ssh: channel open failed\n");
            libssh2_session_free(session);
            close(sock);
            return -1;
        }

        rc = libssh2_channel_shell(channel);
        if (rc) {
            printf("ssh: shell open failed (code=%d)\n", rc);
            libssh2_channel_close(channel);
            libssh2_channel_free(channel);
            libssh2_session_free(session);
            close(sock);
            return -1;
        }

        /* 设置非阻塞，以便 select 轮询 */
        libssh2_session_set_blocking(session, 0);

        printf("\n--- Interactive SSH session ---\n");
        printf("  Type your commands. Use Ctrl+D or 'exit' to disconnect.\n\n");

        /* stdin 设为非阻塞 */
        int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        /* 把终端设为 raw 模式，让输入字符直接透传 */
        system("stty raw -echo < /dev/tty 2>/dev/null");

        while (1) {
            struct timeval tv;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            FD_SET(STDIN_FILENO, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 50000; /* 50ms */

            int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;
            int sel = select(maxfd + 1, &fds, NULL, NULL, &tv);

            /* 即使 select 没触发，也要尝试从 channel 读，
               因为 libssh2 内部可能已经缓存了数据 */
            {
                char rbuf[4096];
                int n = libssh2_channel_read(channel, rbuf, sizeof(rbuf));
                if (n > 0) {
                    fwrite(rbuf, 1, n, stdout);
                    fflush(stdout);
                } else if (n < 0 && n != LIBSSH2_ERROR_EAGAIN) {
                    /* 通道关闭 */
                    break;
                }
            }

            /* 检查远端是否关闭了通道 */
            if (libssh2_channel_eof(channel)) {
                /* 尝试读完剩余数据 */
                char rbuf[4096];
                int n;
                while ((n = libssh2_channel_read(channel, rbuf, sizeof(rbuf))) > 0) {
                    fwrite(rbuf, 1, n, stdout);
                    fflush(stdout);
                }
                break;
            }

            /* 从 stdin 读输入（如果 select 说有数据） */
            if (sel > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
                char ibuf[1024];
                int n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
                if (n > 0) {
                    /* 写入远端 */
                    int wrote = libssh2_channel_write(channel, ibuf, n);
                    (void)wrote;
                } else if (n == 0) {
                    /* EOF (Ctrl+D) */
                    libssh2_channel_send_eof(channel);
                    break;
                } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    break;
                }
            }

            if (sel < 0) break;
        }

        /* 恢复终端设置 */
        system("stty sane < /dev/tty 2>/dev/null");
        /* 恢复 stdin 阻塞模式 */
        fcntl(STDIN_FILENO, F_SETFL, oldf);
        printf("\n--- SSH session closed ---\n");

        libssh2_channel_send_eof(channel);
        libssh2_channel_wait_eof(channel);
        libssh2_channel_close(channel);
        libssh2_channel_free(channel);
    }

    libssh2_session_free(session);
    close(sock);

    return 0;
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

    /* 交互式 SSH 需要 fork 子进程来独占终端 I/O */
    if (!cmd) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("ssh: fork failed\n");
            return -1;
        }
        if (pid > 0) {
            /* 父进程：等待子进程结束 */
            int status;
            waitpid(pid, &status, 0);
            return 0;
        }
        /* 子进程：继续执行 */
    }

    /* 密码：优先环境变量 SSH_PASS，其次从 stdin 交互读取 */
    char pass[256] = "";
    char *env_pass = getenv("SSH_PASS");
    if (env_pass && env_pass[0]) {
        strncpy(pass, env_pass, sizeof(pass) - 1);
    } else {
        /* 隐藏终端读密码 */
        printf("Password for %s@%s: ", user, host);
        fflush(stdout);
        FILE *tty = fopen("/dev/tty", "r");
        if (tty) {
            /* 关闭回显 */
            system("stty -echo < /dev/tty 2>/dev/null");

            if (fgets(pass, sizeof(pass), tty)) {
                size_t plen = strlen(pass);
                if (plen > 0 && pass[plen - 1] == '\n')
                    pass[plen - 1] = '\0';
            }

            /* 恢复回显 */
            system("stty echo < /dev/tty 2>/dev/null");
            fclose(tty);
        } else {
            /* 回退：直接从 stdin 读 */
            printf("\n");
            if (fgets(pass, sizeof(pass), stdin)) {
                size_t plen = strlen(pass);
                if (plen > 0 && pass[plen - 1] == '\n')
                    pass[plen - 1] = '\0';
            }
        }
        printf("\n");
    }

    printf("Connecting to %s@%s:%d ...\n", user, host, port);
    if (cmd) printf("Command: %s\n", cmd);

    int ret = ssh_connect(host, port, user, pass, cmd);

    /* 如果是 fork 出来的子进程，退出 */
    if (!cmd) {
        _exit(ret == 0 ? 0 : 1);
    }

    return ret;
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
