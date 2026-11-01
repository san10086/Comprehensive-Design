# 无显示设备时的 ARM Qt 验证路径

> 2026-11-01 整理更新：当前构建与部署按 [XFTP 更新编译打包说明书](空间处理与Qt更新编译打包说明书_XFTP.md) 操作；最新结果以 [构建与板端回传](evidence/arm-build-2026-11-01/README.md) 为准。用户已确认 ARM 构建、VNC 中文与波形显示及一次重连；HDMI/鼠标、长期运行、真实音频和显示/DSP 共存仍待验收。下文早期环境探测结果按历史阶段阅读。

日期：2026-11-01。适用当前 `frontend_arm/` 工程。本文补充 [环境配置报告](arm-qt-environment-setup-report.md)，不表示已在用户虚拟机或板端执行验证。

## 当前板端回传结果（2026-11-01）

用户通过板端终端提供了以下实际结果；这些是用户执行的证据，不是本机代为运行：

- 架构：`armv7l`；板端 IP：`192.168.1.21`。
- Qt 安装目录：`/usr/local/Trolltech/QtEmbedded-4.6.4-arm`；有 QtCore/QtGui 4.6.4 和 analogclock/wiggly 示例。
- 插件目录中未见 gfxdrivers，已搜索目录未找到 VNC 插件文件。
- analogclock 使用 `-qws -display VNC:0` 启动后输出 `VNC: driver not found` 并 Aborted；没有新增 5900 监听，时钟进程已退出。
- 内存回传包含 `MemFree: 4900 kB`、`Cached: 41544 kB`、`SReclaimable: 12060 kB`，不能只按 MemFree 判断可用内存；本次直接报错为显示驱动不可用。

Qt4 路线结论：**该 Qt4.6.4 运行环境无法直接提供 VNC 显示。** 后续已改用板端现有 Qt5.9.7，并完成下述启动验证；Qt4 示例命令仍需对应 Qt4 驱动支持。

Ubuntu20 后续回传已定位以下候选环境：

- 开发机为 `x86_64`，当前 PATH 中 qmake 为 `/usr/bin/qmake`，其版本/用途尚待核对。
- EZSDK 位于 `/home/user/ti-ezsdk_dm816x-evm_5_05_01_04`，`linux-devkit/bin/qmake` 已确认是 32 位 Intel 80386 宿主机工具，Qt 版本及目标配置尚未核对。
- ARM 编译器为 `/home/user/arm-2014.05/bin/arm-none-linux-gnueabi-g++`，用户已用它成功链接 Qt5.9.7 显示探针；同时存在另一套 CodeSourcery 工具链，本次未选用。
- EZSDK 的 `filesystem/ezsdk-dm816x-evm-rootfs/home/qt_arm/qt5.9.7_install/bin/qmake` 已确认是 x86-64 宿主机工具，配套 `libQt5Core.so.5` 是 32 位 ARM EABI5 库，qconfig.pri 记录版本 5.9.7。
- Qt5 安装中的平台目录包含 `libqvnc.so`、`libqlinuxfb.so`、`libqoffscreen.so`、`libqminimal.so`。用户已在板端确认 `/home/qt_arm/qt5.9.7_install`、Qt5Core 5.9.7 库及上述插件都存在。

用户已运行 Qt5 qmake，确认可在 Ubuntu 启动、版本为 5.9.7。安装/宿主机前缀均仍为 `/home/qt_arm/qt5.9.7_install`，`QT_SYSROOT` 为空。目标 mkspec 为 `linux-TIarmv7-g++`，使用 `arm-none-linux-gnueabi-` 工具链，release 配置为 ARMv7-A / Cortex-A8 / NEON / softfp。VNC 插件已确认 ARM EABI5；moc/rcc/uic 为 x86-64。板端没有 ldd。

用户随后按 [当前环境的 Qt5 VNC 探针操作步骤](qt5-vnc-probe-runbook.md) 完成 Ubuntu 上的 SDK 前缀映射，qmake 和 make 均成功。构建日志使用 `arm-none-linux-gnueabi-g++` 和 ARMv7-A/Cortex-A8/NEON/softfp 标志，链接 Qt5Widgets/Qt5Gui/Qt5Core；产物位于 `/home/user/qt-vnc-probe/build-qt597-arm/qt_syslink_probe`，`file` 确认为 32 位 ARM EABI5 ELF，动态解释器为 `/lib/ld-linux.so.3`。

用户已通过 Ubuntu `192.168.1.41:8000` 将 24807 字节探针下载到板端 `/home/root/qt-vnc-probe/qt_syslink_probe`；板端加载器 `/lib/ld-linux.so.3` 指向 `ld-2.18.so`。启动日志明确出现 `loaded library .../libqvnc.so`、`QVncServer created on port 5900`、`QT_HEARTBEAT ... ticks=10 clicks=0`；netstat 确认 5900 LISTEN。原始回传已保存为 [板端首次启动日志](evidence/qt-vnc-probe-2026-11-01/board-first-start.log)。

用户随后在 Windows 从以太网地址 `192.168.1.43` 对板端 5900 测试：第一次 True，第二次 False，ping 仍可达。当前应记录为“瞬时 TCP 连接已成功、稳定连接未通过”，不能认定远程显示可用。Qt 官方 v5.9.7 源码的客户端断开处理存在未判空的光标清理路径，与短连接测试后服务消失的时序高度吻合；实际板端崩溃/退出原因仍待捕获。后续按 [探针操作步骤第 6 节](qt5-vnc-probe-runbook.md#6-windows连接并验收) 前台运行并直接连接 VNC，暂停纯 TCP 端口探测。

**已通过：Qt5 探针交叉构建、板端初次启动、VNC 插件加载及初次端口监听。待验证/定位：服务稳定性、客户端显示持续刷新、远程点击及正常退出/重启。** 一条心跳只证明事件循环已经运行，不能替代两分钟刷新验收。日志中的 `qt_syslink_probe ... not a plugin` 是插件扫描程序文件时的诊断，本次并未阻止 VNC 插件加载。此次结果不代表整个业务 GUI、DSP 或真实 HDMI 已验证。

若上述 Qt5 运行环境最终不能复用，再检查原 Qt4 SDK/源码和构建配置，决定补部署匹配插件或重新构建启用 VNC 的 Qt。Qt4 官方 VNC 文档给出的内建驱动选项为 `-qt-gfx-vnc`，应加入完整的原交叉编译配置，不能只带这个选项重新 configure。构建或部署新的运行库时使用独立安装目录验证，保留现有运行库。

## 1. 没有显示器，程序会发生什么

没有接 HDMI 显示器，不等于 Linux 没有显示设备。当前 GUI 创建 `QApplication` 和 `MainWindow`，默认全屏；Qt4 启动脚本使用 `-qws`，Qt5 默认使用 `linuxfb`。

| 实际环境 | 可能表现 |
|---|---|
| 没接显示器，但 framebuffer、分辨率和显示后端可正常初始化 | 程序可能正常创建窗口、处理事件和绘制，只是没有物理画面可看 |
| 缺少 framebuffer、驱动未初始化、权限不足或无法设置显示模式 | 显示后端可能初始化失败，报错退出或无法绘制；具体取决于 Qt/驱动版本 |
| 使用桌面 Qt 的 X11/xcb 后端，但没有 X server | 通常不能建立显示连接，需要桌面会话、Xvfb 或其他可用平台后端 |
| 显式选择已经构建并部署的 VNC/offscreen 后端 | 可以绕过物理显示输出，按对应方式运行 |

Qt5 LinuxFB 实现需要打开 framebuffer 并读取设备参数，因此是否能无屏启动取决于驱动提供的设备状态，不能仅由“有没有插显示器”判断。[Qt 官方 LinuxFB 源码](https://github.com/qt/qtbase/blob/5.12/src/plugins/platforms/linuxfb/qlinuxfbscreen.cpp)

当前程序不会因没有屏幕自动变成命令行服务，也不会在启动后自动运行 IPC 或音频。即使 `ps` 能看到 GUI，也不能据此认定按钮、绘制、事件循环及业务均正常。`--windowed` 只改变窗口模式，不取消显示后端需求。

先在板端检查：

```sh
# [DM8168 Linux；只读检查]
cat /proc/fb
ls -l /dev/fb*
cat /proc/cmdline
dmesg | tail -n 80
find /usr /opt /lib \( -name 'libQtCore.so*' -o -name 'libQt5Core.so*' \
    -o -name 'libqvnc.so*' -o -name 'libqoffscreen.so*' \
    -o -name 'libqgfxvnc.so*' \) -print 2>/dev/null
```

Qt 安装在其他目录时另查该目录。没找到插件 `.so` 不一定表示不支持，因为驱动也可能编入 Qt；最终核对 SDK 配置及实际启动日志。

## 2. 推荐的验证顺序及证据边界

| 路径 | 程序执行端 | 能确认什么 | 不能据此确认什么 |
|---|---|---|---|
| 当前 Qt Core 集成测试 | Ubuntu；也可交叉编译后在 ARM | 参数校验、进程控制、停止/异常处理、真实 Linux IPC | 窗口外观、HDMI、真实 DSP/ALSA |
| 同一新工程的桌面 GUI | Ubuntu20 | 可视操作、布局、按钮与 IPC 交互 | ARM ABI、目标驱动及 DSP |
| ARM GUI + VNC | DM8168；Windows/Ubuntu 查看和点击 | 真实 ARM 运行库、窗口交互、板端 IPC；满足硬件前提后可操作真实音频/DSP | HDMI 输出、USB 鼠标驱动、LinuxFB/VPSS 显示共存 |
| ARM GUI + offscreen | DM8168 | GUI 初始化及自动化测试所覆盖的行为 | 单靠启动不能确认界面外观、点击或业务；不验证物理显示 |
| QEMU | Ubuntu/Windows 上仿真 | 在合适配置下运行部分 ARM 用户态程序 | 不能把通用 ARM CPU 仿真当成 DM8168、DSP、SysLink、VPSS 的完整仿真 |

建议先做 Qt Core 测试和 Ubuntu 桌面 GUI，再优先检查板端是否具备 VNC 后端。这样不需要为了缺显示器而改写业务界面，也不需要重新启用已经停用的 `frontend/`。

## 3. 路径一：现有测试本身就不需要显示设备

`frontend_arm/tests/tests.pro` 仅使用 Qt Core/Test，测试入口是 `QCoreApplication`。它不需要 X server、LinuxFB、HDMI 或 `QT_QPA_PLATFORM=offscreen`。

在已经准备好桌面 Qt5 的 Ubuntu20 普通终端执行；不要 source ARM SDK：

```bash
# [Ubuntu20；路径按实际源码副本调整]
PROJECT_ROOT="$HOME/ex04-arm-work"
cd "$PROJECT_ROOT" || exit 1
BUILD_TARGET=desktop BUILD_DIR="$PROJECT_ROOT/frontend_arm/build-desktop" \
    sh frontend_arm/build.sh /usr/bin/qmake
```

构建成功后：

```bash
# [Ubuntu20；同一终端]
mkdir -p "$PROJECT_ROOT/frontend_arm/build-desktop/tests"
cd "$PROJECT_ROOT/frontend_arm/build-desktop/tests" || exit 1
/usr/bin/qmake "$PROJECT_ROOT/frontend_arm/tests/tests.pro" && make -j2 || exit 1
EX04_IPC_PEER="$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_ipc_peer" \
    ./test_frontend_arm > test-results.txt 2>&1
TEST_RC=$?
cat test-results.txt
printf 'TEST_RC=%s\n' "$TEST_RC"
```

每一步成功再继续。通过标准是测试结果没有失败且返回 0。这里的 loader/host 是测试生成的模拟脚本，实际 Linux 信号量/共享内存由 Qt 与独立 C peer 交互；不要把模拟进程生命周期通过当作真实 DSP 已通过。

若要在 ARM 上运行同一套无屏测试，在已加载匹配 ARM Qt SDK 的 Ubuntu 终端执行：

```bash
# [Ubuntu20；专用 ARM Qt SDK 环境]
PROJECT_ROOT="$HOME/ex04-arm-work"
QMAKE_ARM='/实际路径/ARM-Qt-SDK/bin/qmake'
ARM_TEST_BUILD="$PROJECT_ROOT/frontend_arm/build-arm-core-tests"
mkdir -p "$ARM_TEST_BUILD"
cd "$ARM_TEST_BUILD" || exit 1
"$QMAKE_ARM" "$PROJECT_ROOT/frontend_arm/tests/tests.pro" && make -j2 || exit 1
file ./test_frontend_arm
```

需有该 SDK 的 QtTest 开发模块。确认 ARM ELF 后，通过现有部署通道将 `test_frontend_arm` 复制到板端业务目录，并准备同版本 ARM QtCore/QtTest 运行库及已交叉编译的 `ex04_ipc_peer`。然后执行：

```sh
# [DM8168；先设置匹配 Qt 的运行库环境，且没有其他 EX04 任务运行]
cd /home/root/ex04_syslink_dsp/release || exit 1
EX04_IPC_PEER="$PWD/ex04_ipc_peer" ./test_frontend_arm > /tmp/ex04-core-tests.log 2>&1
TEST_RC=$?
cat /tmp/ex04-core-tests.log
printf 'TEST_RC=%s\n' "$TEST_RC"
```

这比只在 x86 上测试多覆盖真实 ARM 用户态环境；仍然不调用真实 loader/DSP。板端还需可写且允许执行临时测试脚本的 `/tmp`、正常 `/bin/sh` 和测试使用的基本命令；精简 BusyBox 与工具差异可能影响测试脚本，需根据具体失败项区分。

## 4. 路径二：Qt 真正在板上运行，电脑通过 VNC 操作

这是没有板端显示器时最适合人工联调 GUI 的路径：ARM 创建窗口、执行 C 辅助进程及业务程序，电脑只接收画面并发送键鼠事件。无需把业务改回 SSH 控制的旧桌面前端。

先沿用环境报告中的 ARM 部署和 Qt 库/字体设置。以下 Qt4、Qt5 只选实际匹配的一支；首次只点击 IPC 验证，不执行音频任务。

### Qt4/Embedded

前提：该 Qt 构建包含 VNC 图形驱动。Qt 官方 Qt4 文档提供 `-qt-gfx-vnc` 构建选项和 `-display VNC:0` 启动方式；是否在课程 Qt4.6 SDK 中启用，需要实际确认。[Qt 官方 Qt4 VNC 文档](https://doc.qt.io/Archives/qt-4.7/qt-embedded-vnc.html)

```sh
# [DM8168；Qt4/Embedded，运行库环境已设置]
cd /home/root/ex04_syslink_dsp/release || exit 1
QWS_SIZE=1024x768 sh ./run-qt4.sh -display VNC:0 > /tmp/ex04-vnc.log 2>&1
```

### Qt5

前提：目标 Qt5 构建提供匹配的 `vnc` 平台插件及其依赖。不是所有 Qt5 版本/裁剪版 SDK 都带这个插件，不能从 Ubuntu x86 Qt 复制插件到 ARM。QPA 插件须与 Qt 版本匹配。[Qt5 官方 QPA 文档](https://doc.qt.io/archives/qt-5.15/qpa.html)

```sh
# [DM8168；Qt5，运行库/插件/字体环境已设置]
cd /home/root/ex04_syslink_dsp/release || exit 1
QT_QPA_PLATFORM='vnc:size=1024x768:port=5900' \
    sh ./run-qt5.sh > /tmp/ex04-vnc.log 2>&1
```

此参数形式已按 Qt5.12 官方 VNC 实现核对：`size` 控制虚拟屏幕，`port` 控制监听端口；其他 SDK 以其实现为准。[VNC 屏幕源码](https://github.com/qt/qtbase/blob/5.12/src/plugins/platforms/vnc/qvncscreen.cpp)、[VNC 集成源码](https://github.com/qt/qtbase/blob/5.12/src/plugins/platforms/vnc/qvncintegration.cpp)

`run-qt5.sh` 已支持用 `QT_QPA_PLATFORM` 覆盖默认 LinuxFB，无需修改业务代码。若报找不到 `vnc`，用 `QT_DEBUG_PLUGINS=1` 增加加载诊断；插件未构建时需使用同一目标 SDK 补建/部署，单设变量不会生成插件。

### Windows 或 Ubuntu 查看端

打开 VNC 客户端，填写板端实际 IP，端口 `5900`。某些客户端用显示号语法 `板端IP:0`，另一些单独填写端口；不要把 `:5900` 在所有客户端里都理解成端口。

使用开发用隔离网络；Qt 内置 VNC 后端不能默认视为具备认证/加密。如果已有 SSH 通道，可按已有网络策略转发，但隧道本身不会限制板端原 VNC 监听端口的可访问范围。

另开一个板端终端查看日志：

```sh
# [DM8168；第二个终端]
tail -f /tmp/ex04-vnc.log
```

按顺序验收：画面出现 → 时钟更新 → 切换标签 → 信号量 10 条消息 → 共享内存 10 条消息 → 停止后重试 → 正常退出。VNC 窗口关闭只是断开客户端，**需要用界面“退出”结束业务 GUI**。

要通过 VNC 控制真实 DSP，仍须先完成内存、SharedRegion、固件所有权及 SysLink 基线核对。VNC 绕过物理显示输出，不会消除 Linux/VPSS/DSP 的内存冲突。即使 VNC + DSP 一起成功，也只能记录该运行配置通过；不能把它写成 LinuxFB/HDMI 与 DSP 共存通过。

## 5. 路径三：Qt5 offscreen 用于自动化或启动检查

前提是当前 Qt5 提供可用的 `offscreen` 插件；其实现提供窗口/绘制缓冲区与事件分发，适合不接物理屏幕的测试。[Qt 官方 offscreen 源码](https://github.com/qt/qtbase/blob/5.12/src/plugins/platforms/offscreen/qoffscreenintegration.cpp)

```sh
# [DM8168；仅 Qt5，运行库/插件环境已设置]
cd /home/root/ex04_syslink_dsp/release || exit 1
QT_QPA_PLATFORM=offscreen sh ./run-qt5.sh --windowed > /tmp/ex04-offscreen.log 2>&1
```

没有可见窗口是预期行为。当前代码没有自动截图、模拟按钮点击或自动运行音频的入口；上述命令最多作为启动检查，不会自动完成业务验收。仅在尚未启动任何任务的检查场景可用 Ctrl+C 结束；不要用这种方式取代 GUI 音频任务的正常停止。

若需要自动验收完整 GUI，可后续添加专用 QtTest GUI 测试：创建 `MainWindow`、查找控件、模拟点击、等待成功/失败信号、对窗口截图并正常退出。当前现成的 Core 集成测试可以先运行，无需等待这些扩展。不同 offscreen/字体配置的图像也不能当作真实 HDMI 像素输出。

## 6. Ubuntu 桌面、Xvfb 与 QEMU 如何选择

Ubuntu 虚拟机已经有图形桌面时，直接运行同一个新工程的桌面构建即可；Windows 上的虚拟机窗口就是观察界面的位置：

```bash
# [Ubuntu20 图形桌面；已完成第 3 节桌面构建]
PROJECT_ROOT="$HOME/ex04-arm-work"
"$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_frontend_arm" --windowed
```

若 Ubuntu 也只有命令行，可以用 Xvfb 给桌面 Qt 的 xcb 后端提供虚拟 X server。Xvfb 明确支持没有显示硬件或物理输入设备的运行环境。[X.Org Xvfb 手册](https://xorg.freedesktop.org/releases/X11R6.9.0/doc/html/Xvfb.1.html)

```bash
# [Ubuntu20；可选，仅桌面 Qt 构建]
sudo apt install xvfb xauth
PROJECT_ROOT="$HOME/ex04-arm-work"
xvfb-run -a -s '-screen 0 1024x768x24' \
    "$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_frontend_arm" \
    -platform xcb --windowed
```

Xvfb 自身不提供远程查看和点击界面，需要另接测试工具或远程显示方案。它不是板端 `/dev/fb0`，不能拿来证明 LinuxFB 驱动正常。Qt4 的 QVFb 也是开发时的虚拟 framebuffer 技术，依赖本地共享内存，不是直接跨网络控制 ARM GUI 的替代品；本场景优先考虑 VNC。

QEMU 的 ARM 系统仿真必须选择对应板级模型，不能只选一个 Cortex-A8 CPU 就得到 DM8168。Qt 运行和 TI C674 DSP/SysLink/Notify/VPSS 仿真是不同问题；若没有这些硬件模型，就不具备完整系统等效性。用户态仿真也不补齐目标驱动和远端 DSP。[QEMU 官方 ARM 仿真说明](https://www.qemu.org/docs/master/system/target-arm.html)

## 7. 无屏阶段可以怎样写验收结论

逐项记录，不使用笼统的“Qt 已完全验证”：

- Ubuntu Core/IPC 测试：通过/失败/未执行，附日志。
- Ubuntu 桌面 GUI：通过/失败/未执行，附截图和操作记录。
- ARM Core/IPC 测试：通过/失败/未执行，附 Qt/ABI/运行库版本。
- ARM VNC GUI：通过/失败/未执行，注明远程输入和虚拟显示后端。
- ARM 实际 DSP/音频：通过/失败/未执行，注明固件及内存配置；独立报告数据校验、输出 WAV 或音频结果。
- 真实 HDMI、USB 输入、显示固件与 DSP 共存：没有相应设备与测试证据时，保持“未验证”。

无屏条件下可以继续完成软件功能和大量板端业务验证；真实显示输出链路的验收留待具有相应观察手段时补做。
