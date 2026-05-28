#include "kernel_log.h"
#include <stdarg.h>
#include <time.h>

static log_level_t s_current_level = LOG_INFO;

static const char *s_level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

void log_set_level(log_level_t level)
{
    s_current_level = level;
}

log_level_t log_get_level(void)
{
    return s_current_level;
}

void log_print(log_level_t level, const char *file, int line,
               const char *func, const char *fmt, ...)
{
    if (level < s_current_level)
        return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    /* 打印时间戳和级别 */
    fprintf(stderr, "[%02d:%02d:%02d] [%s] %s:%d %s(): ",
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            s_level_names[level], file, line, func);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
