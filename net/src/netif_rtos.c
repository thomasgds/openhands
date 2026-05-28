#include "netif_rtos.h"
#include "kernel_log.h"
#include "tap.h"
#include "task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

/* TAP 设备配置 */
#define TAP_DEVICE     "tap0"
#define TAP_ADDR       "10.0.2.2"
#define TAP_NETMASK    "255.255.255.0"
#define TAP_GATEWAY    "10.0.2.1"

#define ETHERNET_MTU   1518

/* lwIP core lock stub */
static pthread_mutex_t s_lwip_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_tap_fd = -1;
static bool s_net_running = false;

/* TCP/IP 协议栈状态（模拟） */
typedef struct {
    char ifname[16];
    char ipaddr[16];
    char netmask[16];
    char gateway[16];
    uint8_t mac[6];
    uint32_t packets_rx;
    uint32_t packets_tx;
} netif_state_t;

static netif_state_t s_netif;

void lwip_lock(void)
{
    pthread_mutex_lock(&s_lwip_lock);
}

void lwip_unlock(void)
{
    pthread_mutex_unlock(&s_lwip_lock);
}

static void netif_rx_thread(void *arg)
{
    (void)arg;
    char buf[ETHERNET_MTU];

    LOG_INFO("Network RX thread started (tap_fd=%d)", s_tap_fd);

    while (s_net_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s_tap_fd, &rfds);

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ret = select(s_tap_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        int n = tap_read(s_tap_fd, buf, sizeof(buf));
        if (n > 0) {
            lwip_lock();
            s_netif.packets_rx++;
            /* 以太网帧接收 — 在实际 lwIP 集成中会调用 netif->input() */
            /* 目前只是统计计数 */
            lwip_unlock();
        }
    }
}

void netif_rtos_poll(void)
{
    /* 在上层任务中轮询，处理来自协议栈的输出 */
    if (s_tap_fd < 0 || !s_net_running) return;
}

bool netif_is_up(void) { return s_net_running && s_tap_fd >= 0; }
const char *netif_get_ip(void) { return s_netif.ipaddr; }

void lwip_init_rtos(void)
{
    memset(&s_netif, 0, sizeof(s_netif));

    /* 生成 MAC 地址 */
    s_netif.mac[0] = 0x02;
    s_netif.mac[1] = 0x00;
    s_netif.mac[2] = 0x00;
    s_netif.mac[3] = 0x00;
    s_netif.mac[4] = 0x00;
    s_netif.mac[5] = 0x01;

    strncpy(s_netif.ifname, TAP_DEVICE, sizeof(s_netif.ifname) - 1);
    strncpy(s_netif.ipaddr, TAP_ADDR, sizeof(s_netif.ipaddr) - 1);
    strncpy(s_netif.netmask, TAP_NETMASK, sizeof(s_netif.netmask) - 1);
    strncpy(s_netif.gateway, TAP_GATEWAY, sizeof(s_netif.gateway) - 1);

    LOG_INFO("lwIP initialized (netif: %s, IP: %s)", s_netif.ifname, s_netif.ipaddr);
}

void lwip_thread_start(void)
{
    /* 打开 TAP 设备 */
    s_tap_fd = tap_open(TAP_DEVICE);
    if (s_tap_fd < 0) {
        LOG_WARN("TAP device not available, network disabled");
        return;
    }

    s_net_running = true;

    /* 创建接收线程 */
    task_create("net_rx", netif_rx_thread, NULL, 0, TASK_PRIORITY_NORMAL);

    LOG_INFO("lwIP thread started");
}

int netif_send_eth(const void *data, int len)
{
    if (s_tap_fd < 0) return -1;
    int n = tap_write(s_tap_fd, data, len);
    if (n > 0) {
        lwip_lock();
        s_netif.packets_tx++;
        lwip_unlock();
    }
    return n;
}
