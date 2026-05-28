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
bool netif_tap_is_connected(void);
const char *netif_get_ip(void);
const char *netif_get_gateway(void);
const char *netif_get_netmask(void);
uint32_t netif_get_packets_rx(void);
uint32_t netif_get_packets_tx(void);

#ifdef __cplusplus
}
#endif

#endif /* NETIF_RTOS_H */
