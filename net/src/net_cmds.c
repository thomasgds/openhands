#include "kernel_log.h"
#include "netif_rtos.h"
#include <stdio.h>
#include <string.h>

void net_cmd_ifconfig(void)
{
    printf("Network Interface:\n");
    printf("  %-12s IP: %s\n", "tap0", netif_get_ip());
    printf("  %-12s Netmask: %s\n", "", netif_get_netmask());
    printf("  %-12s Gateway: %s\n", "", netif_get_gateway());

    if (netif_tap_is_connected()) {
        printf("  Status: UP (TAP connected)\n");
        printf("  Packets: RX=%u TX=%u\n",
               netif_get_packets_rx(), netif_get_packets_tx());
    } else {
        printf("  Status: UP (TAP unavailable)\n");
        printf("  Hint: Run 'sudo scripts/setup_tap.sh' to enable TAP\n");
        printf("  Or: sudo modprobe tun && sudo ip tuntap add tap0 mode tap\n");
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
