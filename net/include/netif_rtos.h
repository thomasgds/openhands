#ifndef NETIF_RTOS_H
#define NETIF_RTOS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void lwip_init_rtos(void);
void lwip_thread_start(void);
void netif_rtos_poll(void);
void lwip_lock(void);
void lwip_unlock(void);
int netif_send_eth(const void *data, int len);
bool netif_is_up(void);
const char *netif_get_ip(void);

#ifdef __cplusplus
}
#endif

#endif /* NETIF_RTOS_H */
