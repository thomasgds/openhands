#include "timer.h"
#include "kernel_log.h"
#include <stdlib.h>
#include <stdbool.h>



void timer_init(void)
{
    LOG_INFO("Timer manager initialized");
}

soft_timer_t *timer_create(const char *name, uint32_t period_ms,
                           int type, timer_callback_t cb, void *arg)
{
    soft_timer_t *t = calloc(1, sizeof(soft_timer_t));
    if (t) {
        t->name = name; t->period_ms = period_ms;
        t->type = type; t->callback = cb; t->arg = arg;
    }
    return t;
}
void timer_start(soft_timer_t *t) { t->active = true; t->remaining_ms = t->period_ms; }
void timer_stop(soft_timer_t *t) { t->active = false; }
void timer_reset(soft_timer_t *t) { t->remaining_ms = t->period_ms; }
void timer_tick(uint32_t elapsed_ms) { (void)elapsed_ms; }
