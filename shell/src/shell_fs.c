#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

/*
 * 文件系统命令：ls, cat, mkdir, rm, cp, mv, pwd, cd
 *
 * 这些命令操作宿主 Linux 文件系统（通过 POSIX API）
 */

static char s_cwd[512] = "/workspace/project";

/* pwd */
static int cmd_pwd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("%s\n", s_cwd);
    return 0;
}

/* cd */
static int cmd_cd(int argc, char **argv)
{
    if (argc < 2) {
        strcpy(s_cwd, "/");
        return 0;
    }

    char path[512];
    if (argv[1][0] == '/') {
        strncpy(path, argv[1], sizeof(path) - 1);
    } else {
        snprintf(path, sizeof(path), "%s/%s", s_cwd, argv[1]);
    }

    /* 规范化路径 */
    char *norm = realpath(path, NULL);
    if (!norm) {
        printf("cd: %s: %s\n", argv[1], strerror(errno));
        return -1;
    }

    struct stat st;
    if (stat(norm, &st) < 0 || !S_ISDIR(st.st_mode)) {
        printf("cd: %s: Not a directory\n", argv[1]);
        free(norm);
        return -1;
    }

    strncpy(s_cwd, norm, sizeof(s_cwd) - 1);
    free(norm);
    return 0;
}

/* ls */
static int cmd_ls(int argc, char **argv)
{
    const char *path = s_cwd;
    int show_all = false;
    int long_fmt = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i] + 1; *p; p++) {
                if (*p == 'a') show_all = true;
                else if (*p == 'l') long_fmt = true;
            }
        } else {
            path = argv[i];
        }
    }

    char fullpath[512];
    if (path[0] != '/') {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", s_cwd, path);
        path = fullpath;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        printf("ls: %s: %s\n", path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.')
            continue;

        if (long_fmt) {
            struct stat st;
            char fullentry[1024];
            snprintf(fullentry, sizeof(fullentry), "%s/%s", path, entry->d_name);

            char perms[11] = "----------";
            char mtime[20] = "Jan 1 00:00";
            off_t size = 0;
            nlink_t nlink = 1;

            if (stat(fullentry, &st) == 0) {
                perms[0] = S_ISDIR(st.st_mode) ? 'd' :
                           S_ISLNK(st.st_mode) ? 'l' : '-';
                perms[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
                perms[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
                perms[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
                perms[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
                perms[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
                perms[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
                perms[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
                perms[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
                perms[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
                size = st.st_size;
                nlink = st.st_nlink;

                struct tm *tm = localtime(&st.st_mtime);
                static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                               "Jul","Aug","Sep","Oct","Nov","Dec"};
                snprintf(mtime, sizeof(mtime), "%s %2d %02d:%02d",
                         months[tm->tm_mon], tm->tm_mday,
                         tm->tm_hour, tm->tm_min);
            }

            printf("%s %3lu %-8s %-8s %8ld %s %s\n",
                   perms, (unsigned long)nlink, "root", "root",
                   (long)size, mtime, entry->d_name);
        } else {
            printf("%s  ", entry->d_name);
        }
    }

    if (!long_fmt) printf("\n");
    closedir(dir);
    return 0;
}

/* cat */
static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        return -1;
    }

    char path[512];
    if (argv[1][0] == '/')
        strncpy(path, argv[1], sizeof(path) - 1);
    else
        snprintf(path, sizeof(path), "%s/%s", s_cwd, argv[1]);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("cat: %s: %s\n", argv[1], strerror(errno));
        return -1;
    }

    char buf[4096];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(f);
    return 0;
}

/* mkdir */
static int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: mkdir <directory>\n");
        return -1;
    }

    char path[512];
    if (argv[1][0] == '/')
        strncpy(path, argv[1], sizeof(path) - 1);
    else
        snprintf(path, sizeof(path), "%s/%s", s_cwd, argv[1]);

    if (mkdir(path, 0755) < 0) {
        printf("mkdir: %s: %s\n", argv[1], strerror(errno));
        return -1;
    }
    return 0;
}

/* rmdir — 删除空目录 */
static int cmd_rmdir(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: rmdir <directory>\n");
        return -1;
    }

    char path[512];
    if (argv[1][0] == '/')
        strncpy(path, argv[1], sizeof(path) - 1);
    else
        snprintf(path, sizeof(path), "%s/%s", s_cwd, argv[1]);

    if (rmdir(path) < 0) {
        printf("rmdir: %s: %s\n", argv[1], strerror(errno));
        return -1;
    }
    return 0;
}

/* rm — 删除文件或目录 */
static int cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: rm [-rf] <file|directory>\n");
        return -1;
    }

    int recursive = false;
    int force = false;
    const char *target = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i] + 1; *p; p++) {
                if (*p == 'r') recursive = true;
                else if (*p == 'f') force = true;
            }
        } else {
            target = argv[i];
        }
    }

    if (!target) {
        printf("rm: missing operand\n");
        return -1;
    }

    char path[512];
    if (target[0] == '/')
        strncpy(path, target, sizeof(path) - 1);
    else
        snprintf(path, sizeof(path), "%s/%s", s_cwd, target);

    struct stat st;
    if (stat(path, &st) < 0) {
        if (!force) printf("rm: %s: %s\n", target, strerror(errno));
        return force ? 0 : -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            printf("rm: %s: is a directory (use -r)\n", target);
            return -1;
        }
        /* 递归删除 */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
        int ret = system(cmd);
        (void)ret;
    } else {
        if (unlink(path) < 0) {
            printf("rm: %s: %s\n", target, strerror(errno));
            return -1;
        }
    }
    return 0;
}

/* cp — 复制文件 */
static int cmd_cp(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: cp <source> <dest>\n");
        return -1;
    }

    char src[512], dst[512];
    if (argv[1][0] == '/')
        strncpy(src, argv[1], sizeof(src) - 1);
    else
        snprintf(src, sizeof(src), "%s/%s", s_cwd, argv[1]);

    if (argv[2][0] == '/')
        strncpy(dst, argv[2], sizeof(dst) - 1);
    else
        snprintf(dst, sizeof(dst), "%s/%s", s_cwd, argv[2]);

    /* 如果目标是目录，在目录下同名 */
    struct stat dst_st;
    if (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode)) {
        const char *basename = strrchr(src, '/');
        if (basename) basename++; else basename = src;
        size_t dlen = strlen(dst);
        snprintf(dst + dlen, sizeof(dst) - dlen, "/%s", basename);
    }

    FILE *fs = fopen(src, "rb");
    if (!fs) {
        printf("cp: %s: %s\n", argv[1], strerror(errno));
        return -1;
    }

    FILE *fd = fopen(dst, "wb");
    if (!fd) {
        printf("cp: %s: %s\n", argv[2], strerror(errno));
        fclose(fs);
        return -1;
    }

    char buf[8192];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
        fwrite(buf, 1, n, fd);
    }

    fclose(fs);
    fclose(fd);
    return 0;
}

/* mv — 移动/重命名文件 */
static int cmd_mv(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: mv <source> <dest>\n");
        return -1;
    }

    char src[512], dst[512];
    if (argv[1][0] == '/')
        strncpy(src, argv[1], sizeof(src) - 1);
    else
        snprintf(src, sizeof(src), "%s/%s", s_cwd, argv[1]);

    if (argv[2][0] == '/')
        strncpy(dst, argv[2], sizeof(dst) - 1);
    else
        snprintf(dst, sizeof(dst), "%s/%s", s_cwd, argv[2]);

    if (rename(src, dst) < 0) {
        printf("mv: %s -> %s: %s\n", argv[1], argv[2], strerror(errno));
        return -1;
    }
    return 0;
}

/* head — 显示文件前几行 */
static int cmd_head(int argc, char **argv)
{
    int lines = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            lines = atoi(argv[++i]);
        } else {
            file = argv[i];
        }
    }

    if (!file) {
        printf("Usage: head [-n <lines>] <file>\n");
        return -1;
    }

    char path[512];
    if (file[0] == '/')
        strncpy(path, file, sizeof(path) - 1);
    else
        snprintf(path, sizeof(path), "%s/%s", s_cwd, file);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("head: %s: %s\n", file, strerror(errno));
        return -1;
    }

    char buf[4096];
    int count = 0;
    while (count < lines && fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
        count++;
    }
    fclose(f);
    return 0;
}

void shell_shell_fs_init(void)
{
    LOG_INFO("shell_fs initialized");
}

void shell_register_fs_cmds(void)
{
    static const shell_cmd_t fs_cmds[] = {
        {"ls",     "List directory contents", cmd_ls},
        {"cat",    "Concatenate and display files", cmd_cat},
        {"mkdir",  "Create directories", cmd_mkdir},
        {"rmdir",  "Remove empty directories", cmd_rmdir},
        {"rm",     "Remove files or directories", cmd_rm},
        {"cp",     "Copy files", cmd_cp},
        {"mv",     "Move/rename files", cmd_mv},
        {"pwd",    "Print working directory", cmd_pwd},
        {"cd",     "Change directory", cmd_cd},
        {"head",   "Display first lines of a file", cmd_head},
    };
    int n = sizeof(fs_cmds) / sizeof(fs_cmds[0]);
    for (int i = 0; i < n; i++)
        shell_register_cmd(&fs_cmds[i]);
    LOG_INFO("File commands registered: %d", n);
}
