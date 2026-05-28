#include "tap.h"
#include "kernel_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int tap_open(const char *dev)
{
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        LOG_ERROR("Failed to open /dev/net/tun: permission denied");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;  /* TAP device, no packet info */

    if (dev)
        strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        LOG_ERROR("Failed to ioctl TUNSETIFF for %s", dev ? dev : "tap");
        close(fd);
        return -1;
    }

    LOG_INFO("TAP device %s opened (fd=%d)", ifr.ifr_name, fd);
    return fd;
}

int tap_read(int fd, void *buf, int len)
{
    return read(fd, buf, len);
}

int tap_write(int fd, const void *buf, int len)
{
    return write(fd, buf, len);
}

int tap_set_addr(int fd, const char *addr)
{
    (void)fd; (void)addr;
    /* 地址配置通常需要外部 ifconfig/ip 命令 */
    LOG_INFO("TAP address would be set to %s (via external ip cmd)", addr);
    return 0;
}
