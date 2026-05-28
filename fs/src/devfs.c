#include "devfs.h"
#include "kernel_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

/* /dev/null - 丢弃所有写入，读取返回 EOF */
static int devnull_open(const char *path, int flags) { (void)path; (void)flags; return 0; }
static int devnull_close(int fd) { (void)fd; return 0; }
static ssize_t devnull_read(int fd, void *buf, size_t count) { (void)fd; (void)buf; (void)count; return 0; }
static ssize_t devnull_write(int fd, const void *buf, size_t count) { (void)fd; (void)buf; (void)count; return count; }

/* /dev/zero - 读取返回零 */
static int devzero_open(const char *path, int flags) { (void)path; (void)flags; return 0; }
static int devzero_close(int fd) { (void)fd; return 0; }
static ssize_t devzero_read(int fd, void *buf, size_t count)
{ (void)fd; memset(buf, 0, count); return count; }
static ssize_t devzero_write(int fd, const void *buf, size_t count) { (void)fd; (void)buf; return count; }

/* /dev/console - 映射到 stdin/stdout */
static int devconsole_open(const char *path, int flags) { (void)path; (void)flags; return 0; }
static int devconsole_close(int fd) { (void)fd; return 0; }
static ssize_t devconsole_read(int fd, void *buf, size_t count)
{
    (void)fd;
    return read(STDIN_FILENO, buf, count);
}
static ssize_t devconsole_write(int fd, const void *buf, size_t count)
{
    (void)fd;
    return write(STDOUT_FILENO, buf, count);
}

/* /dev/random - 简易随机数 */
static int devrandom_open(const char *path, int flags) { (void)path; (void)flags; return 0; }
static int devrandom_close(int fd) { (void)fd; return 0; }
static ssize_t devrandom_read(int fd, void *buf, size_t count)
{
    (void)fd;
    for (size_t i = 0; i < count; i++)
        ((unsigned char *)buf)[i] = (unsigned char)rand();
    return count;
}
static ssize_t devrandom_write(int fd, const void *buf, size_t count) { (void)fd; (void)buf; return count; }

/* 设备路由 */
typedef struct {
    const char *name;
    vfs_ops_t ops;
} dev_entry_t;

static int dev_open(const char *path, int flags);
static int dev_close(int fd);
static ssize_t dev_read(int fd, void *buf, size_t count);
static ssize_t dev_write(int fd, const void *buf, size_t count);

#define NUM_DEVICES 4
static dev_entry_t s_devices[NUM_DEVICES];
static int s_dev_active = 0;

void devfs_init(void)
{
    /* /dev/null */
    s_devices[s_dev_active].name = "null";
    s_devices[s_dev_active].ops.open = devnull_open;
    s_devices[s_dev_active].ops.close = devnull_close;
    s_devices[s_dev_active].ops.read = devnull_read;
    s_devices[s_dev_active].ops.write = devnull_write;
    s_dev_active++;

    /* /dev/zero */
    s_devices[s_dev_active].name = "zero";
    s_devices[s_dev_active].ops.open = devzero_open;
    s_devices[s_dev_active].ops.close = devzero_close;
    s_devices[s_dev_active].ops.read = devzero_read;
    s_devices[s_dev_active].ops.write = devzero_write;
    s_dev_active++;

    /* /dev/console */
    s_devices[s_dev_active].name = "console";
    s_devices[s_dev_active].ops.open = devconsole_open;
    s_devices[s_dev_active].ops.close = devconsole_close;
    s_devices[s_dev_active].ops.read = devconsole_read;
    s_devices[s_dev_active].ops.write = devconsole_write;
    s_dev_active++;

    /* /dev/random */
    s_devices[s_dev_active].name = "random";
    s_devices[s_dev_active].ops.open = devrandom_open;
    s_devices[s_dev_active].ops.close = devrandom_close;
    s_devices[s_dev_active].ops.read = devrandom_read;
    s_devices[s_dev_active].ops.write = devrandom_write;
    s_dev_active++;

    vfs_ops_t devfs_ops;
    memset(&devfs_ops, 0, sizeof(devfs_ops));
    devfs_ops.open = dev_open;
    devfs_ops.close = dev_close;
    devfs_ops.read = dev_read;
    devfs_ops.write = dev_write;

    vfs_mount("/dev/", devfs_ops, NULL);
    LOG_INFO("DevFS initialized (%d devices)", s_dev_active);
}

static dev_entry_t *find_device(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < s_dev_active; i++) {
        if (strcmp(name, s_devices[i].name) == 0)
            return &s_devices[i];
    }
    return NULL;
}

static int dev_open(const char *path, int flags)
{
    /* path is relative after /dev/ */
    if (!path) return -1;
    /* skip leading / */
    while (*path == '/') path++;
    dev_entry_t *dev = find_device(path);
    if (!dev || !dev->ops.open) return -1;
    return dev->ops.open(path, flags);
}

static int dev_close(int fd)
{
    (void)fd;
    return 0;
}

static ssize_t dev_read(int fd, void *buf, size_t count)
{
    (void)fd; (void)buf; (void)count;
    return -1;
}

static ssize_t dev_write(int fd, const void *buf, size_t count)
{
    (void)fd; (void)buf; (void)count;
    return -1;
}
