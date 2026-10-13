# Ivseznayfews

一个从零开始、用 C++ 和 x86 汇编编写的 hobby 操作系统。

这个仓库包含两个版本：

- 64bit/ —— 当前主线，64 位长模式内核
- 32bit/ —— 早期版本，32 位，稳定的命令行 Shell

---

## 64bit 版本

### 已实现

- 64 位长模式（自建页表，GRUB multiboot 直入）
- GDT / IDT / PIC 中断系统
- 键盘驱动、PIT 定时器、串口输出
- 深色图形桌面：侧栏、终端、实时系统状态卡片、鼠标与 F1–F4 导航
- 交互式命令行 Shell（mem / tasks / uptime / memtest）
- 物理内存管理器（4KB 页框、连续页分配、保留页保护）
- 内核堆（16 字节对齐、kmalloc / kfree，仅合并地址连续的空闲块）
- 多任务（轮转调度，通过 iretq 切换上下文）

### 系统扩展 v2.0

默认进入 1024×768 深色桌面，支持 800×600 和文本恢复入口。
鼠标点击左侧 Terminal / System / Help / Files，或按 F1 / F2 / F3 / F4 切换页面；
Tab 轮换页面，Esc 返回终端。终端支持拖动、最小化和 F1 恢复。

- 独立用户页表、Ring 3 ELF 加载、SYSCALL、错误隔离与超时回收。
- ATA PIO、只读 VFS/FAT32：`ls` / `cat readme.txt` / `run hello.elf` / `usertest`。
- E1000、ARP/IPv4/ICMP、UDP 和最小 TCP 诊断客户端：`net` / `ping 10.0.2.2`。
- xAPIC 多核启动、Spinlock 和 AP 工作队列：`cpus` / `smptest`。

实现范围、调用 ABI、限制和复现方法见 [64bit/SYSTEM-V2.md](64bit/SYSTEM-V2.md)。
Shell 仍在内核态；AP 尚不运行抢占式用户任务；FAT32 只读，TCP 尚非完整协议栈。

本机直接运行 `./64bit/run-vm.sh` 可打开 QEMU 测试窗口。
使用说明见 [64bit/VM-TEST.md](64bit/VM-TEST.md)。

### 修复与验证

已修复新任务初始中断帧缺少 RSP/SS、中断调用 C++ 栈对齐和异常错误码布局问题。
默认启动交互式 Shell 与后台调度监控任务，串口每秒输出切换计数。
内核仅使用通用寄存器，当前不支持任务中的浮点/SIMD 状态切换。

自动验证：`cd 64bit && make test`（需要 Python 3）。测试实际启动 QEMU，
检查 PS/2 键盘命令、输入编辑、内存自检及 65 秒持续调度。
`make test-host` 可单独验证分配器。v1.8 将命令执行移到 Shell 任务，
键盘中断只将输入加入队列，避免命令执行阻塞定时器。

### 编译

    cd 64bit
    make
    make run

依赖：g++ (x86_64, GCC 13+)、nasm、ld、grub-mkrescue、xorriso、mtools、qemu-system-x86_64。

---

## 32bit 版本

早期版本。运行在 32 位保护模式，功能较简单，但 Shell 稳定可用。

### 已实现

- Multiboot 引导、GDT / IDT / PIC
- 键盘驱动、交互式 Shell
- 命令：help / clear / echo / about / reboot / shutdown / halt

### 编译

    cd 32bit
    make
    make run

依赖：i686-linux-gnu-g++、nasm、ld、grub-mkrescue、xorriso、qemu-system-i386。

---

## 目录结构

    ivseznayfews/
    ├── 64bit/           64 位版本（当前主线）
    │   ├── boot/        引导与长模式切换
    │   ├── kernel/      内核源码
    │   ├── iso/         GRUB 配置
    │   ├── Makefile
    │   └── linker.ld
    └── 32bit/           32 位版本（早期）
        ├── boot/
        ├── kernel/
        ├── iso/
        ├── Makefile
        └── linker.ld

## License

MIT
