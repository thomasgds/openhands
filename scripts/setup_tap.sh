#!/bin/bash
# setup_tap.sh - 设置 TUN/TAP 网络接口

set -e

TAP_DEV="${1:-tap0}"
TAP_ADDR="${2:-10.0.2.1}"

echo "Setting up TAP device: ${TAP_DEV} (addr: ${TAP_ADDR})"

# 创建 TAP 设备
ip tuntap add dev "${TAP_DEV}" mode tap 2>/dev/null || true

# 启用
ip link set "${TAP_DEV}" up

# 分配 IP 地址
ip addr add "${TAP_ADDR}/24" dev "${TAP_DEV}"

# 开启 IP 转发（可选，宿主机可路由到 RTOS）
echo 1 > /proc/sys/net/ipv4/ip_forward 2>/dev/null || true

echo "TAP device ${TAP_DEV} ready at ${TAP_ADDR}"
echo "RTOS should be reachable at 10.0.2.2"
