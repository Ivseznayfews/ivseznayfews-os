# 2026-10-13 修复验证

- 原版：保留原始内核，仅清理 GRUB 配置以便自动引导。在 QEMU TCG 中复现
  首次任务启动后 RSP=0、页异常、双重异常，最终 Triple fault。
- 修复版：完整生成 ELF/ISO；通过 grub-file Multiboot 检查；链接无 RWX 段警告。
- 实际启动回归通过：PS/2 输入 help，读取 VGA 验证帮助输出；输入 echo ok，
  验证独立输出行；持续运行 65 秒，记录 6601 次调度切换且无异常；
  键盘输入 shutdown 后 QEMU 正常退出。
- 测试环境：QEMU 10.2.1，TCG，128 MB，BIOS 引导，网络关闭。
- 复跑方式：在 64bit 目录执行 make test。验证范围为 64 位主线的上述路径，
  不覆盖真实硬件、32 位分支、浮点/SIMD 上下文和全部内存分配场景。

长模式 IRET 对 SS/RSP 的恢复依据：
[AMD64 Architecture Programmer's Manual, Volume 2](https://docs.amd.com/api/khub/documents/sD1_QL~h4Afq2_tvzxqqSQ/content)。

# 2026-10-13 v1.8 内存管理与 Shell 阶段

- 首次修复已推送 GitHub main：f41a9a1。
- PMM：有效 mmap 记录验证，保留区优先，整页边界处理，内核/引导信息/模块
  保护，连续页搜索，独立分配位图，非法释放拒绝；统计仅计算受管理页池。
- 堆：16 字节对齐，连续页扩展，按地址排序，仅合并物理连续的空闲块，
  分配溢出返回空，错误地址和重复释放不修改计数。
- 键盘 IRQ 只入队，Shell 任务执行命令；255 字符输入队列溢出计数可见。
- 新命令：mem / tasks / uptime / memtest；版本号统一为 v1.8。
- make test-host：实际 PMM/heap 源码的测试通过，含页池耗尽、碎片化、
  非连续堆保护、模块保护、错误释放、整数溢出及 4000 次压力操作。
- QEMU TCG、128 MB、BIOS、关闭网络：help / echo / mem / tasks / uptime /
  memtest / 退格编辑 / shutdown 均通过；65 秒持续调度，记录 6899 次切换，
  没有异常。memtest 实际验证 32 轮内存内容及堆计数恢复。
- 32bit 目录没有修改。验证仍不覆盖真实硬件、SMP、浮点/SIMD 上下文。
- 当前堆释放后保留物理页供复用；未实现虚拟内存扩展或分配代际跟踪。
