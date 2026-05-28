#!/bin/bash
# mkdisk.sh - 创建 FAT32 磁盘镜像
# 用于 RTOS 文件系统后端存储

set -e

IMAGE="disk.img"
SIZE_MB=${1:-64}

echo "Creating ${SIZE_MB}MB FAT32 disk image: ${IMAGE}..."

# 创建空白镜像
dd if=/dev/zero of="${IMAGE}" bs=1M count="${SIZE_MB}" status=progress

# 格式化为 FAT32
mkfs.fat -F32 -n "RTOSDISK" "${IMAGE}"

echo "Done: ${IMAGE} (${SIZE_MB}MB FAT32)"
echo "Use 'mount -o loop ${IMAGE} /mnt' to access on host"
