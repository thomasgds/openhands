#ifndef DEVFS_H
#define DEVFS_H

#include "vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern vfs_ops_t devfs_ops;
void devfs_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DEVFS_H */
