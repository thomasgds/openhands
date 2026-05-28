#include <stdint.h>
#include "hal.h"
#include "kernel_log.h"
#include <stdio.h>
#include <unistd.h>

void hal_init(void) { LOG_INFO("HAL initialized"); }
void hal_console_putchar(char c) { putchar(c); }
char hal_console_getchar(void) { return getchar(); }
void hal_delay_ms(uint32_t ms) { usleep(ms * 1000); }

static uint32_t s_ticks = 0;
uint32_t hal_get_tick_ms(void) { return s_ticks; }
