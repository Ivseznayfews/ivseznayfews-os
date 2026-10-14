# v2.1：多进程、可写 FAT32、多窗口桌面

本阶段同时推进三条主线，并以真实 QEMU 启动和独立磁盘检查验证它们的联动。
仅修改 64 位主线。网络和 AP 工作队列沿用 v2.0；它们的实现范围见 `SYSTEM-V2.md`。

## 进程

最多六个用户进程拥有独立的四级页表、128 KiB 用户区域、Ring 3 中断栈和系统调用栈。
BSP 的定时器轮转调度在切换时更新 CR3、TSS.rsp0 和系统调用上下文；内核任务使用内核页表。
运行期可创建任务，进程结束后释放用户页，保存退出码为 zombie，wait 回收进程槽。
睡眠中的进程被调度器跳过，期限到达或收到 kill 请求后重新就绪。

```
spawn worker.elf
spawn sleep.elf
ps
kill PID
wait PID
proctest
```

将 `PID` 替换为 spawn 返回的数字。`run FILE` 相当于启动后等待退出。
`proctest` 同时占满六个进程槽：四个 worker 在相同虚拟地址保存各自 PID，
经过多次睡眠和抢占后验证内容；另一个访问内核触发隔离，另一个被 kill。
随后连续创建/回收十二个程序，检查用户页没有泄漏，额外创建第七个程序应被拒绝。

仍保留三秒执行期限、64 KiB ELF 限制和 W^X 检查。Shell、驱动与桌面在 Ring 0。
进程只由内核 Shell 创建/等待，本阶段没有 fork、父子权限模型、用户 exec/wait 系统调用、
用户 Shell、SIMD 上下文、跨 AP 进程迁移。wait 期间 Shell 等待，其他用户进程继续运行。

系统调用沿用 RAX 编号，RDI/RSI/RDX 参数、RAX 返回值，RCX/R11 被破坏：

| 编号 | 调用 | 参数 |
| --- | --- | --- |
| 0 | write console | RDI 地址、RSI 长度，最多 4096 字节 |
| 1 | exit | RDI 退出码 |
| 2 | ticks | 返回 100 Hz tick 数 |
| 3 | readfile | RDI 路径、RSI 缓冲区、RDX 容量，最多 4096 字节 |
| 4 | sleep | RDI tick 数，0–300；睡眠期间允许抢占 |
| 5 | getpid | 返回当前 PID |
| 6 | writefile | RDI 路径、RSI 数据、RDX 长度，最多 4096 字节 |

文件路径逐字节检查映射，最多 63 字节，加 NUL 终止符。示例 `worker.elf` 检查进程隔离，
`writer.elf` 通过系统调用写入 USER.TXT；旧的恶意指针/栈/无限循环测试继续保留。

## 可写 FAT32

ATA PIO 增加 WRITE SECTORS 和 FLUSH CACHE。VFS 提供读写和 mkdir，支持根目录与最多
八层路径、8.3 文件名、跨簇文件、目录扩展、空文件和覆盖，单次内核写入最多 64 KiB。
所有 FAT 镜像同步，更新主/备份 FSInfo 空闲簇数；挂载时独立扫描 FAT 得到实际空闲数。
覆盖先写入新簇链、刷新、发布新目录项，再释放旧链，避免正常写入中提前破坏旧内容。

```
mkdir work
write work/note.txt hello
cat work/note.txt
ls work
fstest
run writer.elf
cat user.txt
```

文件系统操作在 BSP 关闭本地中断，避免共享 ATA/FAT 状态被另一进程打断。
AP 不调用 VFS。它是有界的同步实现，尚无异步 I/O、日志恢复、断电原子性、
删除、重命名、长文件名、访问权限、AHCI 或 Ext2。`..` 路径拒绝；目录不能作为文件覆盖。
`fstest` 检查 mkdir、跨簇写入、缩短覆盖、空文件、目录扩展和嵌套路径。

默认镜像改为 `.vm/disk-v2.1.img`，启动器默认持久化写入这个项目内的测试文件。
`VM_SNAPSHOT=1 ./run-vm.sh` 可使用临时写层；`VM_DISK=/absolute/path/new.img` 可指定另一镜像。
生成器只创建新文件，不覆盖已有盘。修改用户程序后，请生成新命名镜像再切换测试盘。
旧版 `.vm/disk-v2.img` 保留。

## 窗口与编辑器

Terminal、System、Help、Files、Editor 五个窗口可以同时存在。
窗口按 z-order 从后往前绘制，图形库裁剪到窗口内；点击按相反顺序命中最上层窗口，
只向有焦点的窗口分发键盘输入。标题栏支持拖动、`_` 最小化和 `X` 关闭。
F1–F5 打开或恢复对应窗口，Tab 轮换，Esc 返回 Terminal。

Files 点击目录进入，Backspace 返回根目录，点击文本文件打开 Editor。
终端 `edit PATH` 可以打开现有文件或新建文档；默认 F5 打开 NOTE.TXT。
Editor 支持末尾输入、Enter、Backspace，F6 或 Ctrl+S 保存，最多 4096 字节。
切换/最小化保留未保存内容；有修改时拒绝关闭或打开另一个文档并提示先保存。
ELF 不由 Files 自动执行或当作文本打开，程序仍通过终端运行。

当前是内核内置窗口合成器和末尾编辑器，没有自由定位文本光标、选择复制、缩放窗口、
Unicode 字体或用户态 GUI API。关闭窗口只是隐藏对应内置工具，不代表终止用户进程。

## 验证入口

在 `64bit` 目录执行：

```bash
make all disk test-host test-graphics
SYSTEM_TEST=1 NETWORK_TEST=1 EXPECT_CPUS=4 \
QEMU_EXTRA='-smp 4 -drive file=.vm/disk-v2.1.img,format=raw,if=ide,snapshot=on -netdev user,id=net0 -device e1000,netdev=net0,romfile=' \
python3 tests/smoke.py
python3 tests/persistence.py
```

`persistence.py` 创建一次性的全新磁盘，实际启动两次：首次写入文件、目录与编辑器内容，
关机后冷启动读取，再通过独立 Python FAT 读取器检查 FAT 镜像一致、FSInfo 计数、
文件内容、目录扩展、簇循环、共享簇与丢失簇。安装了 fsck.fat 时再执行 `fsck.fat -n`。
它不修改用户的默认磁盘。定制 QEMU 时可指定 QEMU/QEMU_EXTRA，但不要在其 QEMU_EXTRA 中附加磁盘。

完整回归另含原有网络、多核分配器测试和 65 秒稳定性观察。
`TEST_PAYLOAD=800x600x32` 或 `TEST_PAYLOAD=text` 检查回退。
本阶段仍以 QEMU BIOS/xAPIC 为验证目标，没有真实硬件或 UEFI 验证。
