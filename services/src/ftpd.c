#include "ftpd.h"
#include "kernel_log.h"
#include "task.h"
#include "vfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define FTP_PORT 21
#define FTP_DATA_PORT_BASE 20000
#define FTP_BACKLOG 5
#define FTP_BUF_SIZE 4096

static int s_ftpd_fd = -1;
static bool s_ftpd_running = false;

/* FTP 会话状态 */
typedef struct {
    int control_fd;
    int data_fd;            /* 主动模式 data socket */
    int data_listen_fd;     /* 被动模式监听 socket */
    int data_port;          /* 主动模式端口 */
    char data_addr[64];     /* 主动模式 IP */
    int pasv_mode;          /* 0=主动(PORT), 1=被动(PASV) */
    char cwd[256];          /* 当前工作目录 */
    char rename_from[256];  /* RNFR 暂存 */
} ftp_state_t;

static void ftp_reply(int fd, int code, const char *msg)
{
    char buf[FTP_BUF_SIZE];
    int n = snprintf(buf, sizeof(buf), "%d %s\r\n", code, msg);
    write(fd, buf, n);
}

/* 打开数据连接 */
static int ftp_open_data(ftp_state_t *st)
{
    if (st->pasv_mode) {
        /* 被动模式：等待客户端连接 */
        if (st->data_listen_fd < 0) {
            /* 创建被动监听 */
            st->data_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (st->data_listen_fd < 0) return -1;

            int opt = 1;
            setsockopt(st->data_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(0);  /* 随机端口 */
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(st->data_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                close(st->data_listen_fd);
                st->data_listen_fd = -1;
                return -1;
            }

            /* 获取分配的端口 */
            socklen_t addrlen = sizeof(addr);
            getsockname(st->data_listen_fd, (struct sockaddr *)&addr, &addrlen);
            st->data_port = ntohs(addr.sin_port);

            listen(st->data_listen_fd, 1);
        }

        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int data_fd = accept(st->data_listen_fd, (struct sockaddr *)&client, &clen);
        close(st->data_listen_fd);
        st->data_listen_fd = -1;
        return data_fd;
    } else {
        /* 主动模式：连接到客户端 */
        if (st->data_port == 0) return -1;

        int data_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (data_fd < 0) return -1;

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(st->data_port);
        inet_pton(AF_INET, st->data_addr, &addr.sin_addr);

        if (connect(data_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(data_fd);
            return -1;
        }
        return data_fd;
    }
}

/* 发送目录列表 */
static void ftp_send_listing(int data_fd, const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
        dprintf(data_fd, "drwxr-xr-x 1 root root 0 Jan 1 00:00 .\r\n");
        close(data_fd);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        char perms[11] = "----------";
        char mtime[20] = "Jan 1 00:00";
        off_t size = 0;

        if (stat(fullpath, &st) == 0) {
            /* 权限 */
            perms[0] = S_ISDIR(st.st_mode) ? 'd' : '-';
            perms[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
            perms[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
            perms[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
            perms[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
            perms[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
            perms[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
            perms[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
            perms[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
            perms[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
            perms[10] = '\0';

            size = st.st_size;

            /* 修改时间 */
            struct tm *tm = localtime(&st.st_mtime);
            static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                           "Jul","Aug","Sep","Oct","Nov","Dec"};
            snprintf(mtime, sizeof(mtime), "%s %2d %02d:%02d",
                     months[tm->tm_mon], tm->tm_mday,
                     tm->tm_hour, tm->tm_min);
        } else {
            /* devfs 条目 */
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                perms[0] = 'd';
        }

        dprintf(data_fd, "%s %4d %-8s %-8s %8ld %s %s\r\n",
                perms, 1, "root", "root", (long)size, mtime, entry->d_name);
    }

    closedir(dir);
    close(data_fd);
}

static void ftp_session(void *arg)
{
    int control_fd = (int)(intptr_t)arg;
    ftp_state_t st;
    char buf[FTP_BUF_SIZE];
    char cmd[64], arg_buf[512];

    memset(&st, 0, sizeof(st));
    st.control_fd = control_fd;
    st.data_fd = -1;
    st.data_listen_fd = -1;
    st.cwd[0] = '/';
    st.cwd[1] = '\0';

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

        strcpy(cmd, "");
        arg_buf[0] = '\0';

        if (sscanf(buf, "%63s %511[^\r\n]", cmd, arg_buf) < 1) {
            if (strlen(buf) > 0) {
                strncpy(cmd, buf, sizeof(cmd) - 1);
            } else {
                continue;
            }
        }

        /* --- 认证 --- */
        if (strcmp(cmd, "USER") == 0) {
            ftp_reply(control_fd, 230, "Login successful (anonymous)");
        } else if (strcmp(cmd, "PASS") == 0) {
            ftp_reply(control_fd, 230, "Logged in");
        } else if (strcmp(cmd, "AUTH") == 0) {
            /* TLS 不需要，但返回 234 不会让客户端断开 */
            ftp_reply(control_fd, 234, "AUTH not supported; continuing plaintext");

        /* --- 系统 --- */
        } else if (strcmp(cmd, "SYST") == 0) {
            ftp_reply(control_fd, 215, "UNIX Type: L8");
        } else if (strcmp(cmd, "FEAT") == 0) {
            /* 列出扩展功能 */
            dprintf(control_fd, "211-Features:\r\n");
            dprintf(control_fd, " PASV\r\n");
            dprintf(control_fd, " SIZE\r\n");
            dprintf(control_fd, "211 End\r\n");
        } else if (strcmp(cmd, "OPTS") == 0) {
            ftp_reply(control_fd, 200, "OK");

        /* --- 目录和文件操作 --- */
        } else if (strcmp(cmd, "PWD") == 0) {
            char resp[512];
            snprintf(resp, sizeof(resp), "\"%s\" is current directory", st.cwd);
            ftp_reply(control_fd, 257, resp);
        } else if (strcmp(cmd, "CWD") == 0) {
            if (arg_buf[0] == '/') {
                strncpy(st.cwd, arg_buf, sizeof(st.cwd) - 1);
            } else {
                char tmp[1024];
                snprintf(tmp, sizeof(tmp), "%s/%s", st.cwd, arg_buf);
                /* 规范化路径 */
                char *norm = realpath(tmp, NULL);
                if (norm) {
                    strncpy(st.cwd, norm, sizeof(st.cwd) - 1);
                    free(norm);
                } else {
                    strncpy(st.cwd, tmp, sizeof(st.cwd) - 1);
                }
            }
            ftp_reply(control_fd, 250, "Directory changed");
        } else if (strcmp(cmd, "CDUP") == 0) {
            /* 上一级 */
            char *last = strrchr(st.cwd, '/');
            if (last && last != st.cwd) *last = '\0';
            else if (last == st.cwd && strlen(st.cwd) > 1)
                st.cwd[1] = '\0';
            ftp_reply(control_fd, 250, "Directory changed");
        } else if (strcmp(cmd, "TYPE") == 0) {
            ftp_reply(control_fd, 200, "Type set to I");

        /* --- 数据连接 --- */
        } else if (strcmp(cmd, "PORT") == 0) {
            /* PORT h1,h2,h3,h4,p1,p2 */
            int h[6];
            if (sscanf(arg_buf, "%d,%d,%d,%d,%d,%d",
                       &h[0], &h[1], &h[2], &h[3], &h[4], &h[5]) == 6) {
                snprintf(st.data_addr, sizeof(st.data_addr),
                         "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);
                st.data_port = h[4] * 256 + h[5];
                st.pasv_mode = 0;
                ftp_reply(control_fd, 200, "PORT command successful");
            } else {
                ftp_reply(control_fd, 501, "Invalid PORT format");
            }
        } else if (strcmp(cmd, "PASV") == 0) {
            /* 创建监听，获取地址和端口 */
            int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (listen_fd < 0) {
                ftp_reply(control_fd, 425, "Cannot open data connection");
                continue;
            }

            int opt = 1;
            setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(0);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                close(listen_fd);
                ftp_reply(control_fd, 425, "Cannot open data connection");
                continue;
            }

            socklen_t addrlen = sizeof(addr);
            getsockname(listen_fd, (struct sockaddr *)&addr, &addrlen);
            int port = ntohs(addr.sin_port);

            listen(listen_fd, 1);

            /* 关闭旧监听 */
            if (st.data_listen_fd >= 0) close(st.data_listen_fd);
            st.data_listen_fd = listen_fd;
            st.data_port = port;
            st.pasv_mode = 1;

            /* 获取本机 IP */
            struct sockaddr_in local;
            socklen_t llen = sizeof(local);
            getsockname(control_fd, (struct sockaddr *)&local, &llen);
            uint32_t ip = ntohl(local.sin_addr.s_addr);

            char resp[128];
            snprintf(resp, sizeof(resp),
                     "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
                     (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                     (ip >> 8) & 0xFF, ip & 0xFF,
                     (port >> 8) & 0xFF, port & 0xFF);
            ftp_reply(control_fd, 227, resp);

        /* --- 列表和文件传输 --- */
        } else if (strcmp(cmd, "LIST") == 0 || strcmp(cmd, "NLST") == 0) {
            int data_fd = ftp_open_data(&st);
            if (data_fd < 0) {
                ftp_reply(control_fd, 425, "Cannot open data connection");
                continue;
            }
            ftp_reply(control_fd, 150, "Opening data connection for LIST");

            char list_path[256];
            if (arg_buf[0] == '/')
                strncpy(list_path, arg_buf, sizeof(list_path) - 1);
            else if (arg_buf[0])
                snprintf(list_path, sizeof(list_path), "%s/%s", st.cwd, arg_buf);
            else
                strncpy(list_path, st.cwd, sizeof(list_path) - 1);

            ftp_send_listing(data_fd, list_path);
            ftp_reply(control_fd, 226, "Directory send OK");

        } else if (strcmp(cmd, "RETR") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            int file_fd = open(path, O_RDONLY);
            if (file_fd < 0) {
                ftp_reply(control_fd, 550, "File not found");
                continue;
            }

            /* 获取文件大小 */
            struct stat fst;
            fstat(file_fd, &fst);

            int data_fd = ftp_open_data(&st);
            if (data_fd < 0) {
                close(file_fd);
                ftp_reply(control_fd, 425, "Cannot open data connection");
                continue;
            }

            char resp[128];
            snprintf(resp, sizeof(resp),
                     "Opening data connection for %s (%ld bytes)",
                     arg_buf, (long)fst.st_size);
            ftp_reply(control_fd, 150, resp);

            /* 传输文件 */
            char xfer_buf[8192];
            int total = 0;
            while (1) {
                int r = read(file_fd, xfer_buf, sizeof(xfer_buf));
                if (r <= 0) break;
                int w = write(data_fd, xfer_buf, r);
                if (w <= 0) break;
                total += w;
            }
            close(file_fd);
            close(data_fd);

            snprintf(resp, sizeof(resp),
                     "Transfer complete (%d bytes)", total);
            ftp_reply(control_fd, 226, resp);

        } else if (strcmp(cmd, "STOR") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            int data_fd = ftp_open_data(&st);
            if (data_fd < 0) {
                ftp_reply(control_fd, 425, "Cannot open data connection");
                continue;
            }

            ftp_reply(control_fd, 150, "Opening data connection for STOR");

            int file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_fd < 0) {
                close(data_fd);
                ftp_reply(control_fd, 553, "Cannot create file");
                continue;
            }

            char xfer_buf[8192];
            int total = 0;
            while (1) {
                int r = read(data_fd, xfer_buf, sizeof(xfer_buf));
                if (r <= 0) break;
                int w = write(file_fd, xfer_buf, r);
                if (w <= 0) break;
                total += w;
            }
            close(file_fd);
            close(data_fd);

            char resp[128];
            snprintf(resp, sizeof(resp),
                     "Transfer complete (%d bytes)", total);
            ftp_reply(control_fd, 226, resp);

        } else if (strcmp(cmd, "SIZE") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            struct stat fst;
            if (stat(path, &fst) == 0 && S_ISREG(fst.st_mode)) {
                char resp[64];
                snprintf(resp, sizeof(resp), "%ld", (long)fst.st_size);
                ftp_reply(control_fd, 213, resp);
            } else {
                ftp_reply(control_fd, 550, "Not a regular file");
            }

        } else if (strcmp(cmd, "DELE") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            if (unlink(path) == 0)
                ftp_reply(control_fd, 250, "File deleted");
            else
                ftp_reply(control_fd, 550, "Cannot delete file");

        } else if (strcmp(cmd, "MKD") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            if (mkdir(path, 0755) == 0) {
                char resp[256];
                snprintf(resp, sizeof(resp), "\"%s\" directory created", path);
                ftp_reply(control_fd, 257, resp);
            } else {
                ftp_reply(control_fd, 550, "Cannot create directory");
            }

        } else if (strcmp(cmd, "RMD") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            if (rmdir(path) == 0)
                ftp_reply(control_fd, 250, "Directory removed");
            else
                ftp_reply(control_fd, 550, "Cannot remove directory");

        } else if (strcmp(cmd, "RNFR") == 0) {
            if (arg_buf[0] == '/')
                strncpy(st.rename_from, arg_buf, sizeof(st.rename_from) - 1);
            else
                snprintf(st.rename_from, sizeof(st.rename_from),
                         "%s/%s", st.cwd, arg_buf);
            ftp_reply(control_fd, 350, "Ready for RNTO");

        } else if (strcmp(cmd, "RNTO") == 0) {
            char path[512];
            if (arg_buf[0] == '/')
                strncpy(path, arg_buf, sizeof(path) - 1);
            else
                snprintf(path, sizeof(path), "%s/%s", st.cwd, arg_buf);

            if (rename(st.rename_from, path) == 0)
                ftp_reply(control_fd, 250, "Rename successful");
            else
                ftp_reply(control_fd, 550, "Cannot rename");
            st.rename_from[0] = '\0';

        } else if (strcmp(cmd, "NOOP") == 0) {
            ftp_reply(control_fd, 200, "OK");

        } else if (strcmp(cmd, "QUIT") == 0) {
            ftp_reply(control_fd, 221, "Bye");
            break;
        } else {
            ftp_reply(control_fd, 502, "Command not implemented");
        }
    }

    close(control_fd);
    if (st.data_fd >= 0) close(st.data_fd);
    if (st.data_listen_fd >= 0) close(st.data_listen_fd);
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
