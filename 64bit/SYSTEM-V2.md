# v2.0：64 位系统基础扩展

这一阶段提供可运行的最小实现。启动器使用四核、64 MiB 测试盘和 E1000 NAT 网卡。
32bit 目录没有改动。默认桌面仍可从 GRUB 切换为文本恢复模式。

## 用户空间与系统调用

`run hello.elf` 从 FAT32 读取 ELF64 ET_EXEC，创建独立页表，经 IRETQ 进入
CPL 3。用户地址从 `0x8000000000` 开始；内核恒等映射保持 supervisor-only。
ELF 段按 RX / RW+NX 映射，拒绝 W+X、越界、段页重叠和无效入口；栈有未映射保护区。
TSS 提供 Ring 3 中断栈；SYSCALL 使用独立内核栈，校验用户地址和 IRET 返回状态。
异常和三秒执行期限会回收用户程序并返回 Shell。

调用约定：RAX 为编号，RDI/RSI/RDX 为参数，RAX 返回结果，RCX/R11 被破坏。

| RAX | 操作 | 参数 |
| --- | --- | --- |
| 0 | write | RDI=字符串地址，RSI=长度（最多 4096） |
| 1 | exit | RDI=退出码（低 8 位） |
| 2 | ticks | 返回 100 Hz tick 数 |
| 3 | readfile | RDI=13 字节内 NUL 结尾的 8.3 路径，RSI=缓冲区，RDX=容量（最多 4096） |

`usertest` 覆盖正常执行、非法指针、访问内核导致的页异常、无限循环超时、
非规范用户栈指针。示例源码在 `user/`。单个程序最多 64 KiB ELF 和 128 KiB 用户区域。
当前只有一个前台用户程序；Shell、桌面和驱动仍在 Ring 0，未迁移为用户服务。
系统调用期间关闭本地中断。未实现 fork/exec 进程模型、用户抢占式多进程或 SIMD 状态保存。

## ATA、VFS、FAT32

ATA PIO LBA28 读取 primary master，VFS 将根挂载点的操作分发给 FAT32 后端。
支持 FAT32 superfloppy 或 MBR 的 0B/0C 分区、根目录短文件名、跨簇读取。
`ls`、`cat readme.txt`、`run hello.elf` 分别验证目录、文件和程序加载。

目前只读、单根挂载点、8.3 文件名；没有写入、子目录、长文件名、AHCI 或 Ext2。
`tools/make_disk.py` 生成新的测试镜像，使用排他创建，拒绝覆盖已有文件。
默认 `.vm/disk-v2.img`；QEMU `snapshot=on` 使虚拟机写入只进入临时层。
修改用户示例后请生成新的命名镜像，再自行替换启动器使用的镜像；`make disk` 不覆盖旧盘。

## E1000 与网络

PCI 探测 Intel 8086:100e，MMIO、DMA 收发描述符环，BSP 轮询收包，校验帧长度、
IPv4/ICMP/UDP/TCP 校验和。支持 ARP、IPv4、ICMP echo、UDP 请求/响应和 TCP 诊断客户端。
固定地址 `10.0.2.15/24`，网关 `10.0.2.2`，匹配 QEMU 默认用户网络。

```
net
ping 10.0.2.2
udp 10.0.2.2 19000 hello
tcp 10.0.2.2 19001 hello
```

UDP/TCP 示例需要宿主对应端口运行回显服务；自动测试会临时创建仅监听
127.0.0.1 的服务。没有入站端口转发。TCP 仅支持单连接握手重试、单次请求和
有序响应，结束时发送 RST；尚无完整数据重传、拥塞控制、连接监听和优雅关闭。
没有 DHCP、DNS、IPv6 或 IP 分片重组。已验证 NAT 网关和宿主回显，不代表公网 ICMP 可达。
寄存器/描述符依据 [Intel 8254x 开发手册](https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf)。

## 多核

从 ACPI RSDT/MADT 发现处理器，xAPIC INIT/SIPI 启动最多 8 个 CPU。
AP 使用独立栈和内核页表；共享任务队列使用 acquire/release Spinlock。
PMM 和 heap 同时使用本地中断保护及跨 CPU 锁，避免并发分配损坏。
`cpus` 显示在线核数和各核完成量；`smptest` 分派 128 个工作、测试并发堆分配
及 128000 次受锁保护的累加。测试断言 AP 确实执行了工作。

BSP 保持原有抢占调度；AP 执行非抢占工作队列，空闲时 pause 轮询，尚无
每核中断/TSS、跨核用户任务迁移、负载均衡和省电 idle。仅验证 QEMU xAPIC/BIOS，
没有宣称通用 SMP 抢占调度、x2APIC 或真实硬件支持。

## 桌面

F1 Terminal、F2 System、F3 Help、F4 Files；Tab 轮换，Esc 返回终端。
终端标题栏可在屏幕内拖动，右侧 `_` 最小化，F1 恢复。Files 显示真实 FAT32
根目录和文件大小，底栏显示在线 CPU 数。图形库提供裁剪矩形、ASCII 字体和软件光标。
仍为单活动窗口桌面，无完整重叠窗口合成、中文字体、应用 GUI API 或 DOOM 移植。

## 复现完整测试

依赖 GNU 工具链、NASM、GRUB、xorriso、mtools、Python 3、QEMU。

```bash
make all disk test-host test-graphics
SYSTEM_TEST=1 NETWORK_TEST=1 EXPECT_CPUS=4 \
QEMU_EXTRA='-smp 4 -drive file=.vm/disk-v2.img,format=raw,if=ide,snapshot=on -netdev user,id=net0 -device e1000,netdev=net0,romfile=' \
python3 tests/smoke.py
```

在 `64bit` 目录执行。`TEST_PAYLOAD=800x600x32` 或 `TEST_PAYLOAD=text` 验证回退入口。
`SMOKE_ARTIFACTS=/absolute/path` 保存串口日志和实际 QEMU 截图。
