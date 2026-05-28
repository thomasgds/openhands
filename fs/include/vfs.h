#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_MAX_PATH 256
#define VFS_MAX_FD   64
#define VFS_O_RDONLY 0
#define VFS_O_WRONLY 1
#define VFS_O_RDWR   2
#define VFS_O_CREAT  0x0100
#define VFS_O_TRUNC  0x0200
#define VFS_O_APPEND 0x0400

typedef struct vfs_stat {
    uint32_t size;
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
} vfs_stat_t;

typedef struct vfs_dirent {
    char name[256];
    uint32_t size;
    uint8_t type;
} vfs_dirent_t;

typedef struct vfs_dir {
    int id;
    int index;
    char path[VFS_MAX_PATH];
} vfs_dir_t;

typedef struct {
    int (*open)(const char *path, int flags);
    int (*close)(int fd);
    ssize_t (*read)(int fd, void *buf, size_t count);
    ssize_t (*write)(int fd, const void *buf, size_t count);
    int (*stat)(const char *path, vfs_stat_t *buf);
    vfs_dir_t *(*opendir)(const char *path);
    vfs_dirent_t *(*readdir)(vfs_dir_t *dir);
    int (*closedir)(vfs_dir_t *dir);
    int (*mkdir)(const char *path);
    int (*remove)(const char *path);
    int (*rename)(const char *old, const char *new);
} vfs_ops_t;

typedef struct vfs_mount {
    char prefix[VFS_MAX_PATH];
    vfs_ops_t ops;
    void *fs_data;
    struct vfs_mount *next;
} vfs_mount_t;

void vfs_init(void);
int vfs_mount(const char *prefix, vfs_ops_t ops, void *fs_data);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
ssize_t vfs_read(int fd, void *buf, size_t count);
ssize_t vfs_write(int fd, const void *buf, size_t count);
int vfs_stat(const char *path, vfs_stat_t *buf);
vfs_dir_t *vfs_opendir(const char *path);
vfs_dirent_t *vfs_readdir(vfs_dir_t *dir);
int vfs_closedir(vfs_dir_t *dir);
int vfs_mkdir(const char *path);
int vfs_remove(const char *path);
int vfs_rename(const char *old, const char *new);
void vfs_mount_disk(void);

#ifdef __cplusplus
}
#endif

#endif /* VFS_H */
