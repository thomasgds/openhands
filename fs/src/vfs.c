#include "vfs.h"
#include "kernel_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

static vfs_mount_t *s_mounts = NULL;

typedef struct {
    bool used;
    vfs_mount_t *mount;
    char path[VFS_MAX_PATH];
    int flags;
    void *internal_fd;
} fd_entry_t;

static fd_entry_t s_fd_table[VFS_MAX_FD];

void vfs_init(void)
{
    memset(s_fd_table, 0, sizeof(s_fd_table));
    LOG_INFO("VFS initialized");
}

int vfs_mount(const char *prefix, vfs_ops_t ops, void *fs_data)
{
    vfs_mount_t *m = calloc(1, sizeof(vfs_mount_t));
    if (!m) return -1;
    strncpy(m->prefix, prefix, sizeof(m->prefix) - 1);
    m->ops = ops;
    m->fs_data = fs_data;
    m->next = s_mounts;
    s_mounts = m;
    LOG_INFO("VFS mounted: %s", prefix);
    return 0;
}

static vfs_mount_t *find_mount(const char *path, const char **rel)
{
    if (!path) { *rel = NULL; return NULL; }
    for (vfs_mount_t *m = s_mounts; m; m = m->next) {
        size_t plen = strlen(m->prefix);
        if (strncmp(path, m->prefix, plen) == 0) {
            *rel = path + plen;
            return m;
        }
    }
    *rel = path;
    return s_mounts;
}

static int alloc_fd(void)
{
    for (int i = 0; i < VFS_MAX_FD; i++) {
        if (!s_fd_table[i].used) {
            s_fd_table[i].used = true;
            return i;
        }
    }
    return -1;
}

static void free_fd(int fd)
{
    if (fd >= 0 && fd < VFS_MAX_FD)
        s_fd_table[fd].used = false;
}

int vfs_open(const char *path, int flags)
{
    if (!path) return -1;
    const char *rel = NULL;
    vfs_mount_t *m = find_mount(path, &rel);
    if (!m || !m->ops.open) return -1;

    int internal_fd = m->ops.open(rel ? rel : path, flags);
    if (internal_fd < 0) return -1;

    int fd = alloc_fd();
    if (fd < 0) {
        m->ops.close(internal_fd);
        return -1;
    }
    s_fd_table[fd].mount = m;
    strncpy(s_fd_table[fd].path, path, VFS_MAX_PATH - 1);
    s_fd_table[fd].flags = flags;
    s_fd_table[fd].internal_fd = (void *)(intptr_t)internal_fd;
    return fd;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FD || !s_fd_table[fd].used) return -1;
    vfs_mount_t *m = s_fd_table[fd].mount;
    int ret = -1;
    if (m && m->ops.close)
        ret = m->ops.close((int)(intptr_t)s_fd_table[fd].internal_fd);
    free_fd(fd);
    return ret;
}

ssize_t vfs_read(int fd, void *buf, size_t count)
{
    if (fd < 0 || fd >= VFS_MAX_FD || !s_fd_table[fd].used) return -1;
    vfs_mount_t *m = s_fd_table[fd].mount;
    if (!m || !m->ops.read) return -1;
    return m->ops.read((int)(intptr_t)s_fd_table[fd].internal_fd, buf, count);
}

ssize_t vfs_write(int fd, const void *buf, size_t count)
{
    if (fd < 0 || fd >= VFS_MAX_FD || !s_fd_table[fd].used) return -1;
    vfs_mount_t *m = s_fd_table[fd].mount;
    if (!m || !m->ops.write) return -1;
    return m->ops.write((int)(intptr_t)s_fd_table[fd].internal_fd, buf, count);
}

int vfs_stat(const char *path, vfs_stat_t *buf)
{
    if (!path || !buf) return -1;
    const char *rel = NULL;
    vfs_mount_t *m = find_mount(path, &rel);
    if (!m || !m->ops.stat) return -1;
    return m->ops.stat(rel ? rel : path, buf);
}

vfs_dir_t *vfs_opendir(const char *path)
{
    if (!path) return NULL;
    const char *rel = NULL;
    vfs_mount_t *m = find_mount(path, &rel);
    if (!m || !m->ops.opendir) return NULL;
    return m->ops.opendir(rel ? rel : path);
}

vfs_dirent_t *vfs_readdir(vfs_dir_t *dir)
{
    if (!dir) return NULL;
    return NULL;
}

int vfs_closedir(vfs_dir_t *dir)
{
    if (!dir) return -1;
    return 0;
}

int vfs_mkdir(const char *path)
{
    if (!path) return -1;
    const char *rel = NULL;
    vfs_mount_t *m = find_mount(path, &rel);
    if (!m || !m->ops.mkdir) return -1;
    return m->ops.mkdir(rel ? rel : path);
}

int vfs_remove(const char *path)
{
    if (!path) return -1;
    const char *rel = NULL;
    vfs_mount_t *m = find_mount(path, &rel);
    if (!m || !m->ops.remove) return -1;
    return m->ops.remove(rel ? rel : path);
}

int vfs_rename(const char *oldpath, const char *newpath)
{
    if (!oldpath || !newpath) return -1;
    const char *rel_old = NULL, *rel_new = NULL;
    vfs_mount_t *m = find_mount(oldpath, &rel_old);
    if (!m || !m->ops.rename) return -1;
    find_mount(newpath, &rel_new);
    return m->ops.rename(rel_old ? rel_old : oldpath,
                         rel_new ? rel_new : newpath);
}

void vfs_mount_disk(void)
{
    LOG_INFO("VFS mount disk - no block device backing store");
}
