#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIMER_ONESHOT  0
#define TIMER_PERIODIC 1

typedef void (*timer_callback_t)(void *arg);

typedef struct soft_timer {
    const char *name;
    uint32_t period_ms;
    uint32_t remaining_ms;
    int type;
    bool active;
    timer_callback_t callback;
    void *arg;
    struct soft_timer *next;
} soft_timer_t;

void timer_init(void);
soft_timer_t *timer_create(const char *name, uint32_t period_ms,
                           int type, timer_callback_t cb, void *arg);
void timer_start(soft_timer_t *t);
void timer_stop(soft_timer_t *t);
void timer_reset(soft_timer_t *t);
void timer_tick(uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H */
