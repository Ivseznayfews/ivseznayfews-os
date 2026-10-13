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

# 2026-10-13 v1.9 图形桌面

- 新增 GRUB 帧缓冲协商、32 位 RGB 软件绘图、字体、光标和 PS/2 鼠标输入。
- 深色侧栏、终端、实时状态卡片、System / Help 页面；F1–F3、Tab、Esc 导航。
- make test-host 和 make test-graphics 通过，覆盖显存保留、格式校验、
  通道顺序、pitch、边界裁剪、字体绘制和光标背景恢复。
- QEMU TCG / 128 MB / BIOS：1024×768 模式通过鼠标点击、快捷键、
  全部 Shell 命令和内存自检；65 秒持续调度，记录 7001 次切换，没有异常。
- 800×600 模式及 VGA 文本恢复模式分别通过命令、内存自检与关机测试。
- 已检查实际终端、系统页、帮助页、小分辨率和文本模式截图。
- 图形缓冲约占 3 MB（1024×768）；当前是单桌面、ASCII 字体，无窗口拖动或 Unicode 输入。
- 本次仍仅验证 QEMU BIOS 环境，不代表已验证真实硬件或 UEFI。

# 2026-10-13 v2.0 系统基础扩展

- 最终 ISO SHA-256：`c6454abe4ac4409d680c116004e5ab32c441d4637b4f032c7dff61fa5255edab`。
- 编译无警告；allocator 与 framebuffer 主机测试通过。
- QEMU TCG、128 MB、4 CPU、IDE FAT32 测试盘、E1000 user NAT：完整回归通过。
- 从 FAT32 加载 HELLO.ELF，CPL 3 执行 write/readfile/exit；普通文本拒绝作为 ELF。
- usertest 覆盖非法指针返回错误、内核地址访问隔离、无限循环三秒回收、
  非规范用户栈拒绝返回，全部通过；此后 Shell、内存测试、关机继续正常。
- 串口确认在线 4 核；smptest 校验 128 个工作、128000 次锁保护累加及跨核堆分配；
  自动检查至少一个 AP 确实完成工作。
- ping 10.0.2.2 收到 ICMP 回应；UDP/TCP 到宿主 127.0.0.1 回显服务通过 NAT
  收到 hello。没有验证公网 ping，没有把有限 TCP 客户端当作完整 TCP 栈。
- F4 文件页、PS/2 终端拖动、最小化、F1 恢复通过真实 QEMU 像素断言。
- 最终版完整回归后持续调度 65 秒，共记录 8501 次切换，没有内核异常。
- 800×600 图形回退和文本恢复均通过单核磁盘/用户隔离/多核队列降级自检：
  分别记录 1801、1439 次调度切换，额外稳定观察 3 秒并正常关机。
- 实际截图、串口日志保存在本机忽略目录 `.vm/v2-final`、`.vm/v2-800`、`.vm/v2-text`。
- 范围与尚未实现项见 `SYSTEM-V2.md`。没有真实硬件、UEFI、完整 SMP 用户调度验证。
