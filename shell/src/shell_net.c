#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

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

    /* 打开通道并执行命令 */
    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
    if (!channel) {
        printf("ssh: channel open failed\n");
        libssh2_session_free(session);
        close(sock);
        return -1;
    }

    if (cmd) {
        rc = libssh2_channel_exec(channel, cmd);
    } else {
        rc = libssh2_channel_shell(channel);
    }
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

    /* 交互式 shell 模式（无命令时） */
    if (!cmd) {
        printf("\nssh: interactive shell not supported, use: ssh <host> <cmd>\n");
    }

    /* 关闭 */
    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_close(channel);
    libssh2_channel_free(channel);
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
    char pass[256] = "";
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

    /* 尝试从环境变量或默认密码 */
    char *env_pass = getenv("SSH_PASS");
    if (env_pass) {
        strncpy(pass, env_pass, sizeof(pass) - 1);
    } else {
        /* 默认密码（主要用于演示/测试环境） */
        const char *default_pass = "root";
        strncpy(pass, default_pass, sizeof(pass) - 1);
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
