#ifndef KERNEL_H
#define KERNEL_H

#include "kernel_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 内核初始化与启动 */
void rtos_init(void);
void rtos_start(void);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_H */
