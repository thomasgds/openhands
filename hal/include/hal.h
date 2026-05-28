#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif

void hal_init(void);
void hal_console_putchar(char c);
char hal_console_getchar(void);
void hal_delay_ms(uint32_t ms);
uint32_t hal_get_tick_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */
