#ifndef KERNEL_LOG_H
#define KERNEL_LOG_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别 */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_NONE
} log_level_t;

/* 设置当前日志级别 */
void log_set_level(log_level_t level);

/* 获取当前日志级别 */
log_level_t log_get_level(void);

/* 核心日志打印函数 */
void log_print(log_level_t level, const char *file, int line,
               const char *func, const char *fmt, ...);

/* 宏定义 */
#define LOG_DEBUG(fmt, ...) \
    log_print(LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    log_print(LOG_INFO,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    log_print(LOG_WARN,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
    log_print(LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/* 断言宏 */
#define KASSERT(cond, fmt, ...) do {                                     \
    if (!(cond)) {                                                       \
        log_print(LOG_ERROR, __FILE__, __LINE__, __func__,               \
                  "ASSERT FAILED: " fmt, ##__VA_ARGS__);                 \
        abort();                                                         \
    }                                                                    \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_LOG_H */
