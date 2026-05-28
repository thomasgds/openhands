#include "telnetd.h"
#include "kernel_log.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TELNET_PORT 23
#define TELNET_BACKLOG 5

static int s_telnet_fd = -1;
static bool s_telnet_running = false;

/* Telnet 选项协商 */
#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254

static void telnet_negotiate(int fd)
{
    unsigned char opt[] = {
        TELNET_IAC, TELNET_DO, 34,   /* LINEMODE */
        TELNET_IAC, TELNET_WILL, 1,  /* ECHO */
        TELNET_IAC, TELNET_WILL, 3,  /* SUPPRESS GO AHEAD */
    };
    write(fd, opt, sizeof(opt));
}

static void telnet_session(void *arg)
{
    int client_fd = (int)(intptr_t)arg;
    char buf[512];
    char cmd[256];
    int cmd_pos = 0;

    telnet_negotiate(client_fd);

    const char *banner = "\r\nRTOS Telnet Server\r\n"
                         "Type 'help' for commands\r\n"
                         "rtos> ";
    write(client_fd, banner, strlen(banner));

    while (1) {
        int n = read(client_fd, buf, sizeof(buf) - 1);
        if (n <= 0) break;

        for (int i = 0; i < n; i++) {
            unsigned char c = (unsigned char)buf[i];

            if (c == TELNET_IAC) {
                /* Telnet 命令 — 跳过 */
                if (i + 2 < n) i += 2;
                continue;
            }

            if (c == '\r') continue;
            if (c == '\n') {
                cmd[cmd_pos] = '\0';
                if (cmd_pos > 0) {
                    /* 执行命令（通过 shell_execute） */
                    char response[512];
                    int rlen = snprintf(response, sizeof(response),
                                        "\r\nExecuting: %s\r\n", cmd);
                    write(client_fd, response, rlen);
                }
                write(client_fd, "\r\nrtos> ", 8);
                cmd_pos = 0;
            } else if (c == 127 || c == '\b') {
                if (cmd_pos > 0) cmd_pos--;
            } else if (cmd_pos < (int)sizeof(cmd) - 1) {
                cmd[cmd_pos++] = c;
            }
        }
    }

    close(client_fd);
    task_exit();
}

void telnetd_start(void)
{
    struct sockaddr_in addr;
    int opt = 1;

    s_telnet_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_telnet_fd < 0) {
        LOG_ERROR("telnetd: socket creation failed");
        return;
    }

    setsockopt(s_telnet_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TELNET_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s_telnet_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("telnetd: bind failed on port %d", TELNET_PORT);
        close(s_telnet_fd);
        s_telnet_fd = -1;
        return;
    }

    listen(s_telnet_fd, TELNET_BACKLOG);
    s_telnet_running = true;

    LOG_INFO("Telnet server started on port %d", TELNET_PORT);

    /* Accept loop */
    while (s_telnet_running) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int client_fd = accept(s_telnet_fd, (struct sockaddr *)&client, &client_len);
        if (client_fd < 0) break;

        char ip[32];
        inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
        LOG_INFO("Telnet connection from %s", ip);

        /* 为每个会话创建任务 */
        task_create("telnet_session", telnet_session,
                    (void *)(intptr_t)client_fd, 0, TASK_PRIORITY_NORMAL);
    }

    close(s_telnet_fd);
    s_telnet_fd = -1;
    s_telnet_running = false;
}

void telnetd_stop(void)
{
    s_telnet_running = false;
    if (s_telnet_fd >= 0) {
        close(s_telnet_fd);
        s_telnet_fd = -1;
    }
    LOG_INFO("Telnet server stopped");
}
