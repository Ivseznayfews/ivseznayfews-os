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
- 交互式命令行 Shell
- 物理内存管理器（Bitmap 4KB 页框分配）
- 内核堆（kmalloc / kfree，含块合并）
- 多任务（轮转调度，通过 iretq 切换上下文）

### 已知问题

多任务不稳定。切换几秒后 iretq 返回非法 CS 值，触发通用保护异常。
任务栈疑似被破坏。计划用独立的 TSS 中断栈修复。

### 编译

    cd 64bit
    make
    make run

依赖：g++ (x86_64, GCC 13+)、nasm、ld、grub-mkrescue、xorriso、qemu-system-x86_64。

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
