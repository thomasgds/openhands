#include "kernel_log.h"
#include "netif_rtos.h"
#include <stdio.h>
#include <string.h>

void net_cmd_ifconfig(void)
{
    printf("Network Interface:\n");
    if (netif_is_up()) {
        printf("  %s   IP: %s\n", "tap0", netif_get_ip());
        printf("  Status: UP\n");
    } else {
        printf("  Status: DOWN (no TAP device)\n");
    }
}

int net_cmd_ping(const char *host)
{
    if (!host) {
        printf("Usage: ping <host>\n");
        return -1;
    }
    /* 模拟 ping — 实际会通过 lwIP ICMP 实现 */
    printf("PING %s: not yet implemented (requires lwIP ICMP)\n", host);
    return 0;
}
