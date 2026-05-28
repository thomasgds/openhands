#include "ftpd.h"
#include "kernel_log.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FTP_PORT 21
#define FTP_BACKLOG 5
#define FTP_BUF_SIZE 1024

static int s_ftpd_fd = -1;
static bool s_ftpd_running = false;

static void ftp_reply(int fd, int code, const char *msg)
{
    char buf[FTP_BUF_SIZE];
    int n = snprintf(buf, sizeof(buf), "%d %s\r\n", code, msg);
    write(fd, buf, n);
}

static void ftp_session(void *arg)
{
    int control_fd = (int)(intptr_t)arg;
    char buf[FTP_BUF_SIZE];
    char cmd[64], arg_buf[512];
    int data_fd = -1;
    

    ftp_reply(control_fd, 220, "RTOS FTP Server ready");

    while (1) {
        int n = read(control_fd, buf, FTP_BUF_SIZE - 1);
        if (n <= 0) break;

        buf[n] = '\0';
        /* 去除 \r\n */
        char *nl = strchr(buf, '\r');
        if (nl) *nl = '\0';
        nl = strchr(buf, '\n');
        if (nl) *nl = '\0';

        LOG_INFO("FTP: %s", buf);

        if (sscanf(buf, "%63s %511[^\r\n]", cmd, arg_buf) < 1)
            continue;

        if (strcmp(cmd, "USER") == 0) {
            ftp_reply(control_fd, 230, "Login successful");
        } else if (strcmp(cmd, "PASS") == 0) {
            ftp_reply(control_fd, 230, "Logged in");
        } else if (strcmp(cmd, "SYST") == 0) {
            ftp_reply(control_fd, 215, "UNIX Type: L8");
        } else if (strcmp(cmd, "PWD") == 0) {
            ftp_reply(control_fd, 257, "\"/\" is current directory");
        } else if (strcmp(cmd, "TYPE") == 0) {
            ftp_reply(control_fd, 200, "Type set to I");
        } else if (strcmp(cmd, "PASV") == 0) {
            /* 简化：返回同一 IP，端口 0 表示不支持 */
            ftp_reply(control_fd, 227, "Entering Passive Mode (10,0,2,2,0,0)");
        } else if (strcmp(cmd, "LIST") == 0) {
            ftp_reply(control_fd, 150, "Opening data connection");
            ftp_reply(control_fd, 226, "Directory send OK");
        } else if (strcmp(cmd, "QUIT") == 0) {
            ftp_reply(control_fd, 221, "Bye");
            break;
        } else {
            ftp_reply(control_fd, 502, "Command not implemented");
        }
    }

    close(control_fd);
    if (data_fd >= 0) close(data_fd);
    task_exit();
}

void ftpd_start(void)
{
    struct sockaddr_in addr;
    int opt = 1;

    s_ftpd_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_ftpd_fd < 0) {
        LOG_ERROR("ftpd: socket creation failed");
        return;
    }

    setsockopt(s_ftpd_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(FTP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s_ftpd_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("ftpd: bind failed on port %d", FTP_PORT);
        close(s_ftpd_fd);
        s_ftpd_fd = -1;
        return;
    }

    listen(s_ftpd_fd, FTP_BACKLOG);
    s_ftpd_running = true;
    LOG_INFO("FTP server started on port %d", FTP_PORT);

    while (s_ftpd_running) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int client_fd = accept(s_ftpd_fd, (struct sockaddr *)&client, &clen);
        if (client_fd < 0) break;

        task_create("ftp_session", ftp_session,
                    (void *)(intptr_t)client_fd, 0, TASK_PRIORITY_NORMAL);
    }

    close(s_ftpd_fd);
    s_ftpd_fd = -1;
    s_ftpd_running = false;
}

void ftpd_stop(void)
{
    s_ftpd_running = false;
    if (s_ftpd_fd >= 0) {
        close(s_ftpd_fd);
        s_ftpd_fd = -1;
    }
    LOG_INFO("FTP server stopped");
}
