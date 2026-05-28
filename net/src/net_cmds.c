#include "kernel_log.h"
#include "netif_rtos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

/* ============================================================
 *   Ping 实现 — 使用 Linux 原始 ICMP socket
 *   需要 root 或 cap_net_raw 权限
 * ============================================================ */

/* ICMP 校验和 */
static uint16_t icmp_checksum(uint16_t *addr, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len / 2; i++)
        sum += addr[i];
    if (len & 1)
        sum += ((uint8_t *)addr)[len - 1];
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}

static struct timeval tv_diff(struct timeval *start, struct timeval *end)
{
    struct timeval d;
    d.tv_sec = end->tv_sec - start->tv_sec;
    d.tv_usec = end->tv_usec - start->tv_usec;
    if (d.tv_usec < 0) {
        d.tv_sec--;
        d.tv_usec += 1000000;
    }
    return d;
}

/* Ping 一个主机 */
static int ping_host(struct in_addr addr, int count, int timeout_ms)
{
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        if (errno == EPERM || errno == EACCES) {
            printf("ping: need root or cap_net_raw permission\n");
        } else {
            printf("ping: socket error: %s\n", strerror(errno));
        }
        return -1;
    }

    /* 设置接收超时 */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char send_buf[sizeof(struct icmphdr) + 56];
    char recv_buf[1024];
    int sent = 0, received = 0;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr = addr;

    printf("PING %s (%s): %zu data bytes\n",
           inet_ntoa(addr), inet_ntoa(addr), sizeof(send_buf) - sizeof(struct icmphdr));

    for (int seq = 0; seq < count; seq++) {
        struct icmphdr *icmp = (struct icmphdr *)send_buf;

        memset(send_buf, 0, sizeof(send_buf));
        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->un.echo.id = htons(getpid() & 0xFFFF);
        icmp->un.echo.sequence = htons(seq);
        icmp->checksum = 0;

        /* 填充数据区（时间戳 + 填充） */
        struct timeval *tv_send = (struct timeval *)(send_buf + sizeof(struct icmphdr));
        gettimeofday(tv_send, NULL);

        icmp->checksum = icmp_checksum((uint16_t *)send_buf, sizeof(send_buf));

        ssize_t n = sendto(sock, send_buf, sizeof(send_buf), 0,
                           (struct sockaddr *)&dest, sizeof(dest));

        if (n < 0) {
            printf("ping: sendto error (%s)\n", strerror(errno));
            break;
        }
        sent++;

        /* 对当前 seq: 循环 recv 直到收到匹配的回复或超时 */
        struct timeval deadline;
        gettimeofday(&deadline, NULL);
        deadline.tv_usec += timeout_ms * 1000;
        deadline.tv_sec += deadline.tv_usec / 1000000;
        deadline.tv_usec %= 1000000;

        bool got_reply = false;

        while (!got_reply) {
            struct timeval now;
            gettimeofday(&now, NULL);
            long left_ms = (deadline.tv_sec - now.tv_sec) * 1000 +
                           (deadline.tv_usec - now.tv_usec) / 1000;
            if (left_ms <= 0) break;

            struct timeval rcv_tv;
            rcv_tv.tv_sec = left_ms / 1000;
            rcv_tv.tv_usec = (left_ms % 1000) * 1000;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof(rcv_tv));

            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);

            n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                         (struct sockaddr *)&from, &fromlen);
            if (n < 0) break;

            struct iphdr *iph = (struct iphdr *)recv_buf;
            int iph_len = iph->ihl * 4;
            struct icmphdr *icmp_reply = (struct icmphdr *)(recv_buf + iph_len);

            /* 忽略自己发出的 ECHO (type=8) */
            if (icmp_reply->type == ICMP_ECHO) continue;
            /* 只处理 ECHO REPLY */
            if (icmp_reply->type != ICMP_ECHOREPLY) continue;
            /* ID 必须匹配 */
            if (icmp_reply->un.echo.id != htons(getpid() & 0xFFFF)) continue;
            /* 只处理当前 seq（或之前的未收包） */
            int reply_seq = ntohs(icmp_reply->un.echo.sequence);
            if (reply_seq != seq) continue;

            got_reply = true;
            received++;

            struct timeval *tv_sent_pkt = (struct timeval *)(recv_buf + iph_len + sizeof(struct icmphdr));
            struct timeval reply_now, diff;
            gettimeofday(&reply_now, NULL);
            diff = tv_diff(tv_sent_pkt, &reply_now);

            printf("%zd bytes from %s: icmp_seq=%d ttl=%d time=%ld.%03ld ms\n",
                   n - iph_len,
                   inet_ntoa(from.sin_addr),
                   reply_seq,
                   iph->ttl,
                   (long)diff.tv_sec * 1000 + diff.tv_usec / 1000,
                   (long)diff.tv_usec % 1000);
        }

        if (!got_reply) {
            printf("Request timeout for icmp_seq=%d\n", seq);
        }

        /* 发送间隔 1 秒 */
        sleep(1);
    }

    close(sock);

    /* 统计 */
    int lost = sent - received;
    float loss_pct = sent > 0 ? (lost * 100.0f / sent) : 0.0f;
    printf("\n--- %s ping statistics ---\n", inet_ntoa(addr));
    printf("%d packets transmitted, %d received, %.0f%% packet loss\n",
           sent, received, loss_pct);

    return received > 0 ? 0 : -1;
}

int net_cmd_ping(const char *host)
{
    if (!host) {
        printf("Usage: ping <host>\n");
        return -1;
    }

    /* 解析主机名或 IP */
    struct in_addr addr;

    /* 尝试点分十进制 */
    if (inet_pton(AF_INET, host, &addr) == 1) {
        return ping_host(addr, 4, 3000);
    }

    /* 尝试 DNS 解析 */
    printf("ping: %s: Name resolution not yet implemented\n", host);
    return -1;
}

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
