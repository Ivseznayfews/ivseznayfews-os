# 本地 ISO 测试虚拟机

在仓库目录运行：

```bash
./64bit/run-vm.sh
```

也可以指定另一个 ISO：

```bash
./64bit/run-vm.sh /absolute/path/to/test.iso
```

使用 QEMU GTK 窗口、四核 x86_64（`VM_CPUS=1` 可改单核）、128 MB 内存、TCG 软件模拟、BIOS
和只读 CD-ROM。附加 `.vm/disk-v2.1.img` FAT32 测试盘（默认持久化；`VM_SNAPSHOT=1` 使用临时写层）及
E1000 用户网络 NAT，不开放入站转发端口。默认启动 v2.1 深色图形桌面。
先执行 `make -C 64bit all disk` 生成 ISO 和用户程序/测试盘。

鼠标点击左侧 Terminal / System / Help / Files / Editor 切换页面，也可按 F1 / F2 / F3 / F4 / F5；F6 或 Ctrl+S 保存编辑器。终端标题栏可拖动，`_` 最小化，F1 恢复。
Tab 轮换页面，Esc 返回终端。切换页面会保留终端内容和正在输入的命令。
顶部卡片实时显示空闲内存、运行时间与调度次数。

启动时有 2 秒 GRUB 菜单，第二项 text recovery 可进入文本恢复模式。

建议在 Terminal 页面依次测试：`help`、`mem`、`tasks`、`uptime`、`memtest`、`echo hello`。
`clear` 清屏，`reboot` 重启，`shutdown` 关闭虚拟机。
若鼠标被窗口捕获，按左 Ctrl + 左 Alt 释放。

串口日志：`64bit/.vm/serial.log`。
本机的临时 QEMU 软件包已保存到 `64bit/.vm/`，无需系统安装，重启宿主后
仍可使用。此目录已被 Git 忽略。若系统安装了 QEMU，脚本优先使用系统版本。

同一时间只启动一个此脚本的实例，共用串口日志和本地 monitor socket。

扩展测试：`ls`、`cat readme.txt`、`run hello.elf`、`usertest`、`cpus`、`smptest`、`net`、`ping 10.0.2.2`。
实现范围及限制见 [SYSTEM-V2.1.md](SYSTEM-V2.1.md)。

新增：`spawn worker.elf` / `ps` / `kill PID` / `wait PID` / `proctest` / `fstest` / `edit note.txt`。
默认写入仅作用于项目测试盘；`VM_DISK` 可指定另外一个镜像文件。
