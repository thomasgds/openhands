# 1. OBJECTIVE

在 x86 Ubuntu 机器上构建一个可在用户态运行的轻量级 RTOS（实时操作系统），包含完整的任务调度内核、网络协议栈（TCP/IP）、虚拟文件系统（VFS）、交互式 Shell 界面，并集成 SSH、FTP、Telnet 等网络服务，支持文件上传/下载和 Shell 重载操作。

# 2. CONTEXT SUMMARY

## 项目性质
- 这是一个**全新的 C 语言项目**，当前工作目录 `/workspace/project` 中尚无任何源代码（仅有一个空的 README.md）。
- 当前开发目标系统是 **x86 Ubuntu**，RTOS 将作为**用户态进程**运行（非虚拟机、非 QEMU），底层通过 Linux 的 TUN/TAP 接口实现网络桥接。
- **远期目标**：RTOS 核心代码（内核调度 + HAL + 网络栈 + 文件系统 + Shell）经过跨平台移植后，可在 **ARM 等嵌入式处理器**上运行，届时替换 HAL 层即可适配真实硬件。

## 核心技术选型
| 模块 | 技术方案 | 理由 |
|------|---------|------|
| 任务调度器 | 自研协作式 + 抢占式混合调度 | 轻量级，无依赖，适合教学和定制 |
| 网络协议栈 | **lwIP** (lightweight IP) v2.1.x | 业界标准的嵌入式 TCP/IP 栈，功能完备，易集成 |
| 文件系统 | **FatFs** + VFS 抽象层 | FatFs 是嵌入式领域最广泛使用的 FAT 文件系统库 |
| Shell 界面 | 自研行式 Shell，支持管道和重定向 | 完全自主可控 |
| SSH 服务 | **Dropbear**（静态编译集成） | 嵌入式 Linux 标准 SSH 服务端，轻量级，广泛用于 ARM 平台，支持 SSH2/SCP/SFTP |
| FTP 服务 | 自研简单 FTP 服务器 | FTP 协议相对简单，可基于 lwIP socket API 实现 |
| Telnet 服务 | 自研 Telnet 服务器 | Telnet 协议简单，适合教学演示 |

## 关键依赖
- `gcc` / `make` / `cmake` — 构建工具链
- Linux TUN/TAP 驱动 — 网络桥接（需 `root` 或 `cap_net_admin` 权限）
- `libpthread` — 用户态多线程（用于模拟 RTOS 任务的并发）
- `libreadline` — Shell 的历史记录和行编辑（可选）
- **Dropbear** 源码（https://github.com/mkj/dropbear） — SSH 服务器，静态编译集成
- **跨平台关注点**：所有硬件相关代码集中在 `hal/` 层，内核 + VFS + lwIP + FatFs + Dropbear 均已有 ARM 移植经验，迁移时仅需重新实现 HAL

## 架构概览
```
┌─────────────────────────────────────────────────────┐
│                    Shell (CLI)                       │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────┐ │
│  │  ls  │ │ cat  │ │ ifconfig│ │ ping │ │ 其他命令 │ │
│  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └────┬─────┘ │
│     │        │        │        │           │        │
├─────┴────────┴────────┴────────┴───────────┴────────┤
│           系统调用接口 (Syscall Interface)              │
├─────┬────────┬────────┬────────┬────────────────────┤
│ 任  │ 同步  │ 内存  │ 定时  │   网络服务层   │
│ 务  │ 原语  │ 管理  │ 器   │ SSH│FTP│Telnet │
│ 调  │(Mutex │ (堆 + │ 管   │   服务器       │
│ 度  │ Sem)  │ 页)   │ 理   │                 │
│ 器  │       │       │      │   lwIP TCP/IP   │
├─────┴───────┴───────┴──────┴────────────────────┤
│          虚拟文件系统 (VFS)                         │
│     ┌──────────┐  ┌──────────┐                   │
│     │  FatFs   │  │  /dev/   │  (设备文件)       │
│     └──────────┘  └──────────┘                   │
├──────────────────────────────────────────────────┤
│          硬件抽象层 (HAL)                          │
│  ┌──────────────┐  ┌─────────────────────────┐  │
│  │ TUN/TAP 驱动 │  │ 磁盘镜像文件 I/O         │  │
│  └──────────────┘  └─────────────────────────┘  │
└──────────────────────────────────────────────────┘
```

## 运行时环境
- 程序编译为 Linux ELF 可执行文件
- 磁盘镜像（如 `disk.img`）用于持久化文件系统
- TUN/TAP 设备用于网络接入，RTOS 内部获得独立 IP（如 10.0.2.x）
- Shell 可通过 stdin/stdout 交互，也可通过 Telnet/SSH 远程登录

## 跨平台移植策略
| 层 | x86 Ubuntu（开发阶段） | ARM 嵌入式（未来移植） |
|----|----------------------|----------------------|
| 任务调度 | 基于 `ucontext` / `setjmp`/`longjmp` 模拟上下文切换 | 基于硬件 PendSV/SVC 中断 + 栈指针切换 |
| 时钟源 | `setitimer` + `SIGALRM` | 硬件 Timer 中断 |
| 控制台 I/O | stdin/stdout / PTY | UART 驱动 |
| 网络接口 | TUN/TAP 虚拟设备 | 以太网 MAC + PHY 驱动（如 STM32 ETH / LAN8720） |
| 磁盘 I/O | 宿主机文件模拟块设备 | SPI Flash / SD卡 / eMMC 驱动 |
| 内核 + VFS + lwIP + FatFs + Dropbear | **代码完全不修改**，这些层已经是纯 C、无平台依赖 | 直接复用 |

# 3. APPROACH OVERVIEW

## 整体策略：分层递进式构建

采用**自底向上、分层构建**的方式，在 Github 仓库中分阶段迭代开发。每个阶段都产出可运行、可测试的增量版本。

## 阶段划分（共 6 个主要阶段）

```
Phase 0: 构建系统 & 项目骨架
Phase 1: RTOS 内核核心（任务调度 + 同步原语 + 内存管理 + 定时器）
Phase 2: 虚拟文件系统（VFS + FatFs + 磁盘镜像）
Phase 3: 网络协议栈（lwIP 集成 + TUN/TAP 驱动）
Phase 4: Shell 命令解释器（内置命令 + 管道/重定向）
Phase 5: 网络服务（Telnet / FTP / SSH 服务器）
Phase 6: 集成测试与完善
```

## 为什么选择这个方案
1. **lwIP + FatFs** — 嵌入式领域久经考验的组件组合，API 成熟，文档齐全
2. **用户态运行** — 无需内核模块或虚拟机，开发调试方便
3. **TUN/TAP 桥接** — 让 RTOS 拥有独立的虚拟网络接口，同时利用宿主机网络栈
4. **分层架构** — 各模块松耦合，便于独立测试和扩展
5. **增量交付** — 每个阶段都有可运行的可执行文件和演示场景

# 4. IMPLEMENTATION STEPS

## Phase 0: 构建系统 & 项目骨架

### Step 0.1: 初始化项目结构
- **目标**: 创建标准化的 C 项目目录结构
- **方法**:
  ```bash
  /workspace/project/
  ├── CMakeLists.txt          # 顶层构建文件
  ├── README.md
  ├── kernel/                 # RTOS 内核
  │   ├── CMakeLists.txt
  │   ├── include/            # 对外头文件
  │   └── src/
  ├── fs/                     # 文件系统
  │   ├── include/
  │   └── src/
  ├── net/                    # 网络栈
  │   ├── include/
  │   ├── src/
  │   └── lwip/               # lwIP 源码子模块
  ├── shell/                  # Shell 解释器
  │   ├── include/
  │   └── src/
  ├── services/               # 网络服务
  │   ├── include/
  │   └── src/
  ├── hal/                    # 硬件抽象层
  │   ├── include/
  │   └── src/
  └── fatfs/                  # FatFs 源码
      ├── include/
      └── src/
  ```
- **产出**: `cmake -B build && cmake --build build` 可正常编译（输出空 main）

### Step 0.2: 实现日志与调试基础设施
- **目标**: 提供统一的日志打印、断言、错误码机制
- **方法**: 在 `kernel/src/kernel_log.c` 中实现分级日志（DEBUG/INFO/WARN/ERROR）和 assert 宏
- **参考文件**: `kernel/include/kernel_log.h`

---

## Phase 1: RTOS 内核核心

### Step 1.1: 任务调度器
- **目标**: 实现轻量级任务（线程）调度器，支持协作式和基于时间片的抢占式调度
- **方法**:
  - 定义 `task_t` 结构体（TCB：上下文、栈指针、优先级、状态等）
  - 实现任务创建 API: `task_create(name, func, stack_size, priority)`
  - 实现任务状态管理: READY / RUNNING / BLOCKED / SLEEPING
  - 实现调度器: 优先级优先 + 时间片轮转（Round-Robin）
  - 使用 Linux `setitimer` / `SIGALRM` 模拟时钟中断（每个 tick 1~10ms）
  - 使用 `ucontext` 系列函数或 `setjmp`/`longjmp` 实现上下文切换
- **参考文件**: `kernel/include/task.h`, `kernel/src/task.c`
- **测试**: 创建 3 个任务轮流打印，验证调度正确性

### Step 1.2: 同步原语
- **目标**: 实现 Mutex（互斥锁）、Semaphore（信号量）、Message Queue（消息队列）
- **方法**:
  - `mutex_create/lock/unlock` — 支持优先级继承防反转
  - `sem_create/wait/signal` — 计数信号量
  - `queue_create/send/receive` — FIFO 消息队列
  - 所有原语在内核态操作，任务阻塞时自动让出 CPU
- **参考文件**: `kernel/include/sync.h`, `kernel/src/sync.c`

### Step 1.3: 内存管理
- **目标**: 实现内核堆内存管理（malloc/free 的替代）
- **方法**:
  - 固定大小内存池（kmem_pool）用于小对象分配，减少碎片
  - 通用堆分配器（First-fit 或 Buddy system）用于大对象
  - API: `kalloc(size)`, `kfree(ptr)`, `kalloc_reinit()`
- **参考文件**: `kernel/include/memory.h`, `kernel/src/memory.c`

### Step 1.4: 定时器管理
- **目标**: 实现软件定时器，支持一次性/周期定时
- **方法**:
  - `timer_create(period_ms, callback, oneshot)`
  - `timer_start/stop/reset`
  - 基于系统 tick 的定时器链表管理
- **参考文件**: `kernel/include/timer.h`, `kernel/src/timer.c`

### Step 1.5: 内核初始化与启动
- **目标**: 实现 `rtos_init()` 和 `rtos_start()` 启动流程
- **方法**:
  - 内核初始化调用链：硬件抽象层 → 内存 → 定时器 → 调度器
  - `rtos_start()` 启动系统 tick 并进入第一个用户任务
- **参考文件**: `kernel/src/kernel.c`

---

## Phase 2: 虚拟文件系统

### Step 2.1: 集成 FatFs 库
- **目标**: 将 FatFs（ff13c 或更新版本）源码纳入项目并编译
- **方法**:
  - 将 FatFs 源码放入 `fatfs/` 目录
  - 实现 FatFs 需要的底层磁盘 I/O 接口: `disk_read`, `disk_write`, `disk_ioctl`, `disk_status`, `disk_initialize`
  - 磁盘 I/O 后端是一个文件（如 `disk.img`），用 Linux 文件模拟块设备
- **参考文件**: `fatfs/src/diskio.c`（用户实现层）, `fatfs/src/ff.c`（FatFs 核心）

### Step 2.2: 创建磁盘镜像工具
- **目标**: 提供工具脚本创建/格式化 FAT 磁盘镜像
- **方法**:
  - 编写 `tools/mkdisk.sh` 脚本，用 `dd` 创建空镜像并用 `mkfs.fat` 格式化
  - 默认创建 64MB FAT32 镜像
  - 提供将宿主文件拷贝入镜像的辅助脚本

### Step 2.3: 实现 VFS 抽象层
- **目标**: 在 FatFs 之上封装统一的虚拟文件系统接口
- **方法**:
  - 定义通用 `vfs_file_t` 和文件操作函数指针结构:
    ```c
    typedef struct {
        int (*open)(const char *path, int flags);
        int (*close)(int fd);
        ssize_t (*read)(int fd, void *buf, size_t count);
        ssize_t (*write)(int fd, const void *buf, size_t count);
        int (*stat)(const char *path, struct vfs_stat *buf);
        DIR* (*opendir)(const char *path);
        // ...
    } vfs_ops_t;
    ```
  - 注册表机制：不同路径前缀挂载不同文件系统（如 `/fat/` → FatFs，`/dev/` → 设备文件）
  - API: `vfs_open`, `vfs_read`, `vfs_write`, `vfs_close`, `vfs_mkdir`, `vfs_listdir`
- **参考文件**: `fs/include/vfs.h`, `fs/src/vfs.c`

### Step 2.4: 设备文件系统
- **目标**: 实现 `/dev/` 下的设备文件（null, zero, random, console）
- **方法**:
  - 注册 `/dev/` 挂载点，指向 `devfs_ops`
  - 实现 `devfs_open/read/write` 以支持设备文件语义
  - `/dev/console` 映射到 RTOS Shell 的 stdin/stdout
- **参考文件**: `fs/src/devfs.c`

---

## Phase 3: 网络协议栈

### Step 3.1: 集成 lwIP
- **目标**: 将 lwIP v2.1.x 源码纳入项目并配置
- **方法**:
  - 下载 lwIP 源码或作为 git submodule 引入
  - 编写 `lwipopts.h` 配置（启用 TCP、UDP、ICMP、DHCP、Socket API）
  - 实现 lwIP 需要的底层网络接口函数: `low_level_init`, `low_level_output`, `low_level_input`
  - 实现系统时钟接口 `sys_now()` 返回毫秒级时间
- **参考文件**: `net/include/lwipopts.h`, `net/src/netif_rtos.c`

### Step 3.2: TUN/TAP 驱动
- **目标**: 实现通过 Linux TUN/TAP 设备桥接 RTOS 网络
- **方法**:
  - 创建 TAP 设备（Layer 2）或 TUN 设备（Layer 3）
  - 配置 lwIP 的 netif 接口绑定到 TAP 设备
  - 实现读写循环：从 TAP fd 读取以太网帧 → 送给 lwIP 输入，从 lwIP 输出 → 写入 TAP fd
  - 在宿主机上配置 TAP 接口 IP 和路由（使用脚本）
- **关键文件**: `hal/src/tap.c`, `net/src/netif_rtos.c`
- **依赖**: 运行需要 `sudo ./scripts/setup_tap.sh` 创建 TAP 设备

### Step 3.3: 网络配置命令
- **目标**: 实现网络接口配置和状态查看
- **方法**:
  - ifconfig 命令：显示/设置 IP、子网掩码、MAC
  - ping 命令：基于 lwIP ICMP echo API
  - dhcp 命令：启停 DHCP 客户端（lwIP 内置）
- **参考文件**: `net/src/net_cmds.c`

### Step 3.4: lwIP Socket API 封装
- **目标**: 为上层服务提供标准的 Socket 风格 API
- **方法**:
  - 封装 lwIP 的 `netconn` API 为类 BSD Socket 接口
  - `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`
  - 使其与 POSIX socket 语义兼容，方便移植服务
- **参考文件**: `net/include/lwip_socket.h`, `net/src/lwip_socket.c`

---

## Phase 4: Shell 命令解释器

### Step 4.1: 命令行基础框架
- **目标**: 实现基础的 Shell 事件循环，支持命令解析和执行
- **方法**:
  - `shell_init()` 初始化 Shell 任务
  - 读取输入行（支持退格、删除、光标移动）
  - 解析命令为 argc/argv（处理引号转义）
  - 命令查找表 `cmd_t builtin_cmds[]`
  - 执行命令并输出结果
  - 支持 `help` 命令列出所有可用命令
- **参考文件**: `shell/include/shell.h`, `shell/src/shell_core.c`

### Step 4.2: 内置文件操作命令
- **目标**: 实现文件系统操作命令
- **命令列表**:
  - `ls [path]` — 列出目录内容
  - `cd [path]` — 切换工作目录
  - `pwd` — 显示当前路径
  - `cat <file>` — 显示文件内容
  - `echo <text> [> file]` — 输出文本（支持重定向）
  - `mkdir <path>` — 创建目录
  - `rm <path>` — 删除文件/目录
  - `cp <src> <dst>` — 复制文件
  - `mv <src> <dst>` — 移动/重命名文件
- **参考文件**: `shell/src/shell_fs.c`

### Step 4.3: 系统管理命令
- **目标**: 实现系统监控和管理命令
- **命令列表**:
  - `ps` — 列出运行中任务及其状态
  - `kill <task_id>` — 终止指定任务
  - `meminfo` / `free` — 显示内存使用情况
  - `uptime` — 显示系统运行时间
  - `reboot` — 重启系统（重置状态）
  - `date` — 显示当前日期时间
- **参考文件**: `shell/src/shell_sys.c`

### Step 4.4: 网络命令
- **目标**: 实现网络相关命令
- **命令列表**:
  - `ifconfig [interface]` — 显示/配置网络接口
  - `ping <host>` — ICMP Ping 测试
  - `netstat` — 显示网络连接状态
  - `dhcp [start|stop]` — DHCP 客户端控制
  - `route` — 显示路由表
- **参考文件**: `shell/src/shell_net.c`

### Step 4.5: 输入/输出重定向和管道
- **目标**: 支持 `>`, `>>`, `<`, `|` 操作
- **方法**:
  - 解析命令时检测 `>` / `>>` / `<` / `|` 符号
  - `>` / `>>` 将 stdout 重定向到文件
  - `<` 将 stdin 重定向为文件
  - `|` 将前一个命令的 stdout 连接到后一个命令的 stdin（通过内核消息队列）
- **参考文件**: `shell/src/shell_io.c`

### Step 4.6: Shell 重载
- **目标**: 实现 `reload` 命令重新加载配置文件或重置 Shell
- **方法**:
  - `reload` 命令：从配置文件（如 `/etc/shellrc`）重新加载别名和初始化脚本
  - 支持 `.rc` 启动脚本（启动时自动执行）
  - 支持 `source <script>` 命令执行脚本文件
- **参考文件**: `shell/src/shell_reload.c`

---

## Phase 5: 网络服务

### Step 5.1: Telnet 服务器
- **目标**: 实现 Telnet 服务器，允许远程登录 RTOS Shell
- **方法**:
  - 基于 lwIP Socket API 创建 TCP 监听（端口 23）
  - 接受客户端连接后，分配一个新的 Shell 实例绑定到该连接
  - 处理 Telnet 协议选项协商（NAWS, linemode, echo）
  - 支持多个并发 Telnet 会话（每个会话一个独立任务）
- **参考文件**: `services/telnetd.c`, `services/include/telnetd.h`
- **测试**: 宿主机 `telnet 10.0.2.x 23` 连接

### Step 5.2: FTP 服务器
- **目标**: 实现 FTP 服务器，支持文件上传和下载
- **方法**:
  - 基于 lwIP Socket API 创建 TCP 监听（端口 21）
  - 处理 FTP 协议命令: USER, PASS, SYST, PWD, CWD, LIST, PASV, RETR, STOR, DELE, MKD, RMD, RNFR, RNTO
  - 支持匿名登录（`anonymous`）和本地账户登录
  - PASV 模式：动态分配数据连接端口
  - 文件操作通过 VFS 层进行
  - 支持多个并发 FTP 会话
- **参考文件**: `services/ftpd.c`, `services/include/ftpd.h`
- **测试**: `ftp 10.0.2.x` 连接，执行 `get` / `put` / `ls` / `mkdir`

### Step 5.3: SSH 服务器（集成 Dropbear）
- **目标**: 将 **Dropbear** SSH 服务器静态编译集成到 RTOS，提供加密的远程 Shell 访问和 SCP 文件传输
- **Dropbear 简介**: 嵌入式 Linux 的标准 SSH 服务端，代码量约 100KB，广泛用于 OpenWrt、DD-WRT、各种 ARM 嵌入式系统。支持 SSH2、SCP、SFTP，依赖 libtomcrypt 和 libtommath 两个轻量级密码库。
- **集成方法**:
  1. **获取源码**: 将 Dropbear 源码（https://github.com/mkj/dropbear）作为 git submodule 引入，路径 `third_party/dropbear/`
  2. **编译为静态库**: 配置 Dropbear 编译选项，生成静态库 `.a` 文件
     - 禁用 PAM 认证（RTOS 使用本地密码认证）
     - 启用最小化编译（`--disable-zlib` 可选，减少依赖）
     - 配置交叉编译预留（未来 ARM 移植时更换工具链）
  3. **Socket 适配层**: Dropbear 原生使用 POSIX socket API，需要编写适配层将 POSIX socket 调用桥接到 lwIP socket API
     - 实现 `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `close()`
     - 实现 `read()`, `write()`, `select()`（映射到 lwIP 的 `netconn` 或 `lwip_socket`）
     - 适配层文件: `services/dropbear_sock.c`, `services/include/dropbear_sock.h`
  4. **密码认证**: 实现 Dropbear 的认证回调，使用 RTOS 本地用户数据库（简单文本配置 `/etc/passwd`）
  5. **Shell 绑定**: SSH 会话验证后，将 Dropbear 的 `child_process` stdout/stdin 绑定到 RTOS Shell 实例
  6. **SCP 支持**: Dropbear 内置 `scp` 二进制支持，通过 VFS 层读写文件
  7. **服务任务**: 创建 RTOS 任务运行 Dropbear 主循环，监听端口 22
- **参考文件**: `services/sshd.c`, `services/include/sshd.h`, `services/dropbear_sock.c`
- **跨平台说明**: Dropbear 本身就是为 ARM/MIPS/RISC-V 等嵌入式架构设计的，移植到 ARM 仅需更换编译器，代码无需修改
- **测试**: `ssh user@10.0.2.x` 连接并执行命令，`scp file user@10.0.2.x:~/` 上传文件

### Step 5.4: 服务管理框架
- **目标**: 统一管理网络服务的启动、停止和监控
- **方法**:
  - `service start <name>` / `service stop <name>` / `service status`
  - 支持开机自启配置
  - 每个网络服务作为一个独立 RTOS 任务运行
  - 记录服务日志到 `/var/log/` 目录
- **参考文件**: `services/svc_mgr.c`

---

## Phase 6: 集成测试与完善

### Step 6.1: 系统启动流程整合
- **目标**: 确保所有模块按正确顺序初始化
- **方法**:
  - 完整的启动序列: HAL → 内存 → 定时器 → 调度器 → VFS → 挂载磁盘 → 网络初始化 → Shell → 启动网络服务
  - 启动时可执行 `/etc/init.rc` 脚本

### Step 6.2: 网络连通性测试
- **目标**: 验证 RTOS 网络栈与宿主机网络的互通
- **方法**:
  - 从 RTOS ping 宿主机（10.0.2.1 → 宿主机 TAP 接口 IP）
  - 从宿主机 ping RTOS（10.0.2.2 → RTOS 网络接口 IP）
  - DHCP 客户端自动获取 IP

### Step 6.3: 文件传输测试
- **目标**: 验证通过 FTP 和 SSH 的文件上传/下载
- **方法**:
  - 使用 `ftp` 客户端上传文件到 RTOS，确认文件写入磁盘镜像
  - 使用 `ftp` 客户端从 RTOS 下载文件，验证内容一致
  - 使用 `scp` 通过 SSH 传输文件
  - 在 RTOS Shell 中使用 `ls`/`cat` 验证文件完整性

### Step 6.4: 远程 Shell 测试
- **目标**: 验证 Telnet 和 SSH 远程登录功能
- **方法**:
  - `telnet 10.0.2.2 23` 登录，执行文件操作命令
  - `ssh user@10.0.2.2` 登录，执行命令
  - 验证多会话并发支持

### Step 6.5: 压力测试与稳定性
- **目标**: 验证系统在持续运行和高负载下的稳定性
- **方法**:
  - 多任务并发（10+ 任务）下长时间运行
  - 大文件 FTP 上传/下载（>10MB）
  - 多个 Telnet/SSH 会话同时连接
  - 检查内存泄漏和资源泄漏

---

## 项目文件清单（最终完整结构）

```
/workspace/project/
├── CMakeLists.txt                  # 顶层构建
├── README.md
├── scripts/
│   ├── setup_tap.sh               # TAP 设备配置脚本
│   ├── mkdisk.sh                  # 磁盘镜像创建脚本
│   └── run_rtos.sh                # 一键启动脚本
├── kernel/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── kernel.h               # 主头文件
│   │   ├── kernel_log.h           # 日志系统
│   │   ├── task.h                 # 任务调度 API
│   │   ├── sync.h                 # 同步原语 API
│   │   ├── memory.h               # 内存管理 API
│   │   └── timer.h                # 定时器 API
│   └── src/
│       ├── kernel.c               # 内核初始化
│       ├── task.c                 # 任务调度实现
│       ├── sync.c                 # 同步原语实现
│       ├── memory.c               # 内存管理实现
│       └── timer.c                # 定时器实现
├── fs/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── vfs.h                  # VFS 接口
│   │   └── devfs.h                # 设备文件系统
│   └── src/
│       ├── vfs.c                  # VFS 实现
│       ├── devfs.c                # 设备文件系统实现
│       └── fatfs_drv.c            # FatFs 磁盘驱动
├── fatfs/
│   ├── include/
│   │   ├── ff.h                   # FatFs API (原版)
│   │   ├── diskio.h               # FatFs 磁盘 I/O 接口 (原版)
│   │   └── ffconf.h               # FatFs 配置
│   └── src/
│       ├── ff.c                   # FatFs 核心（原版源码）
│       ├── ffunicode.c            # FatFs Unicode 支持（原版源码）
│       └── diskio.c               # 磁盘 I/O 实现（用户实现层）
├── net/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── lwipopts.h             # lwIP 配置
│   │   ├── lwip_socket.h          # Socket API 封装
│   │   └── netif_rtos.h           # 网络接口声明
│   ├── src/
│   │   ├── netif_rtos.c           # lwIP 网络接口
│   │   ├── lwip_socket.c          # Socket API 封装
│   │   └── net_cmds.c             # 网络命令实现
│   └── lwip/                      # lwIP 源码（submodule 或直接包含）
├── shell/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── shell.h
│   └── src/
│       ├── shell_core.c           # Shell 主循环
│       ├── shell_cmds.c           # 命令注册表
│       ├── shell_fs.c             # 文件命令
│       ├── shell_sys.c            # 系统命令
│       ├── shell_net.c            # 网络命令
│       ├── shell_io.c             # 重定向与管道
│       └── shell_reload.c         # reload 功能
├── services/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── telnetd.h
│   │   ├── ftpd.h
│   │   ├── sshd.h
│   │   ├── svc_mgr.h
│   │   └── dropbear_sock.h        # Dropbear POSIX→lwIP 适配层
│   └── src/
│       ├── telnetd.c              # Telnet 服务器
│       ├── ftpd.c                 # FTP 服务器
│       ├── sshd.c                 # SSH 服务器主控
│       ├── dropbear_sock.c        # Dropbear socket 适配层
│       └── svc_mgr.c              # 服务管理器
├── hal/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── hal.h                  # HAL 通用接口
│   │   ├── uart.h                 # UART/控制台接口
│   │   └── tap.h                  # TUN/TAP 接口
│   └── src/
│       ├── hal_console.c          # 控制台 I/O
│       └── tap.c                  # TUN/TAP 驱动
├── third_party/                   # 第三方源码（git submodules）
│   ├── lwip/                      # lwIP TCP/IP 协议栈
│   ├── fatfs/                     # FatFs 文件系统库
│   └── dropbear/                  # Dropbear SSH 服务端
│       ├── libtomcrypt/           # 密码学库（Dropbear 依赖）
│       └── libtommath/            # 大数运算库（Dropbear 依赖）
├── main.c                         # 入口
├── porting_guide.md               # ARM 移植指南文档
└── tools/
    ├── setup_tap.sh
    ├── mkdisk.sh
    └── run_rtos.sh
```

## 跨平台移植说明
- 详见项目根目录下的 `porting_guide.md`，该文档将在 Phase 6 编写
- 核心原则：所有平台相关代码集中在 `hal/` 层，其他模块（kernel/fs/net/shell/services）是纯 C、零平台依赖
- ARM 移植时新建 `hal/arm/` 目录，实现 `hal.h` 定义的接口即可

# 5. TESTING AND VALIDATION

## 单元测试

每个模块提供独立测试程序，位于各模块的 `tests/` 子目录：

| 模块 | 测试内容 | 验证方式 |
|------|---------|---------|
| 内核调度 | 任务创建、切换、优先级、时间片 | 观察任务执行顺序和时间 |
| 同步原语 | Mutex 互斥、Sem 计数、Queue 收发 | 多任务竞争下的正确性 |
| 内存管理 | 分配/释放/碎片 | 内存泄漏检测、压力分配 |
| 定时器 | 一次性/周期性定时回调 | 计时精度验证 |
| VFS | 文件创建/读写/删除 | 验证数据一致性 |
| 网络栈 | TCP/UDP 收发 | 环回测试 |

## 集成测试脚本

```bash
# 1. 构建项目
./scripts/build.sh

# 2. 创建磁盘镜像
./scripts/mkdisk.sh

# 3. 设置 TAP 网络
sudo ./scripts/setup_tap.sh

# 4. 启动 RTOS
./scripts/run_rtos.sh

# 5. 在另一个终端进行功能验证
# Telnet 测试
telnet 10.0.2.2 23

# FTP 测试
ftp 10.0.2.2

# SSH 测试
ssh admin@10.0.2.2

# Ping 测试
ping 10.0.2.2
```

## 验证标准

| 功能 | 成功标准 |
|------|---------|
| 任务调度 | 至少 10 个任务并发运行不崩溃 |
| 文件系统 | 创建、读写、删除 1000 个文件无错误 |
| 网络连通 | RTOS ↔ 宿主机双向 ping 成功 |
| Telnet | 远程登录 shell，可执行所有命令 |
| FTP | 上传和下载 100MB 文件，MD5 校验一致 |
| SSH | 加密登录，远程执行命令，SCP 传输文件 |
| Shell | 管道、重定向、脚本执行均正常工作 |
| Shell 重载 | `reload` 命令成功重载配置 |
| 稳定性 | 持续运行 24 小时无内存泄漏或崩溃 |
