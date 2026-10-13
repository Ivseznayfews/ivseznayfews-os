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
- 交互式命令行 Shell（新增 mem / tasks / uptime / memtest）
- 物理内存管理器（4KB 页框、连续页分配、保留页保护）
- 内核堆（16 字节对齐、kmalloc / kfree，仅合并地址连续的空闲块）
- 多任务（轮转调度，通过 iretq 切换上下文）

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
