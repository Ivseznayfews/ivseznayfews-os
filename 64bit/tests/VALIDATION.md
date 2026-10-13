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
