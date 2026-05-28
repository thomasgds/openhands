#!/bin/bash
# setup_tap.sh - 设置 TUN/TAP 网络接口
# 在宿主机上以 root 运行: sudo bash scripts/setup_tap.sh

set -e

TAP_DEV="${1:-tap0}"
TAP_ADDR="${2:-10.0.2.1}"

echo "Setting up TAP device: ${TAP_DEV} (addr: ${TAP_ADDR})"

# 加载 TUN 模块
if ! lsmod | grep -q tun; then
    modprobe tun 2>/dev/null || echo "[!] modprobe tun failed (already loaded?)"
fi

# 清理旧的同名设备
ip link show "${TAP_DEV}" &>/dev/null && ip tuntap del "${TAP_DEV}" mode tap

# 创建 TAP 设备
ip tuntap add dev "${TAP_DEV}" mode tap

# 启用
ip link set "${TAP_DEV}" up

# 分配 IP 地址
ip addr add "${TAP_ADDR}/24" dev "${TAP_DEV}"

# 开启 IP 转发（可选，宿主机可路由到 RTOS）
sysctl -w net.ipv4.ip_forward=1 &>/dev/null || true

echo ""
echo "TAP device ${TAP_DEV} ready at ${TAP_ADDR}"
echo "RTOS should be reachable at 10.0.2.2"
echo "Run RTOS: ./rtos.elf"
