#!/bin/bash
# run_rtos.sh - 一键启动 RTOS

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
TAP_DEV="${TAP_DEV:-tap0}"
DISK_IMAGE="${DISK_IMAGE:-${PROJECT_DIR}/disk.img}"

echo "=== RTOS Launcher ==="

# 1. 检查 TAP 设备
if ! ip link show "${TAP_DEV}" &>/dev/null; then
    echo "[!] TAP device ${TAP_DEV} not found."
    echo "    Run: sudo ${SCRIPT_DIR}/setup_tap.sh"
    echo "    Starting RTOS without network..."
fi

# 2. 检查磁盘镜像
if [ ! -f "${DISK_IMAGE}" ]; then
    echo "[!] Disk image ${DISK_IMAGE} not found."
    echo "    Run: sudo ${SCRIPT_DIR}/mkdisk.sh"
    echo "    Starting RTOS without filesystem..."
fi

# 3. 启动 RTOS
echo "Starting RTOS..."
cd "${PROJECT_DIR}"
exec ./rtos.elf "$@"
