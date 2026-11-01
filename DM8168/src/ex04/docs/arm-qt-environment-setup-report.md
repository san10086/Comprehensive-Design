# DM8168 ARM Qt 环境配置与操作报告

> 2026-11-01 整理更新：当前构建与部署按 [XFTP 更新编译打包说明书](空间处理与Qt更新编译打包说明书_XFTP.md) 操作；最新结果以 [构建与板端回传](evidence/arm-build-2026-11-01/README.md) 为准。用户已确认 ARM 构建、VNC 中文与波形显示及一次重连；HDMI/鼠标、长期运行、真实音频和显示/DSP 共存仍待验收。下文早期环境探测结果按历史阶段阅读。

日期：2026-10-31。适用工程：`ex04/frontend_arm/`；开发机：Ubuntu20 虚拟机；运行设备：DM8168 ARM Linux。

本报告按“在哪一端执行 → 使用什么命令 → 怎样算通过”编排。旧桌面前端已清理，新工程继续使用现有 SysLink 音频链路。当前交付的是配置与验收步骤。**2026-11-01 板端回传已确认 Qt4.6.4/Embedded，VNC 试启动报 `VNC: driver not found`；Ubuntu 已定位 EZSDK、ARM GCC、SDK qmake 和一套 Qt5.9.7 安装候选，配套架构/配置及 NFS 映射仍待核对，不能把本文当成已经完成的板端验收记录。**

2026-11-01 补充：当前没有显示设备，可先按 [无显示设备时的 ARM Qt 验证路径](arm-qt-headless-validation.md) 完成无屏测试或 VNC 联调；本文涉及 HDMI 观察的验收项暂记“未验证”。

## 1. 先分清三端及两套 Qt

| 端 | 打开什么终端 | 负责什么 | 不在这里做什么 |
|---|---|---|---|
| Windows 主机 | PowerShell、VS Code、串口软件 | 编辑当前工程、传文件、连接板端控制台 | 不用 Windows Qt 编译板端程序 |
| Ubuntu20 虚拟机 | Ubuntu 的 Terminal / Bash | 桌面调试、ARM 交叉编译、DSP 编译、向 rootfs 部署 | 不把 x86 桌面 Qt 程序复制到板上运行 |
| DM8168 板端 | 串口或现有远程终端，进入板子的 Linux shell | 运行 Qt、SysLink、ALSA；HDMI 显示；USB 鼠标输入 | 不在 U-Boot 提示符下执行 Linux 命令，不在板端安装 Ubuntu 开发包 |

即使串口软件开在 Windows，登录后输入的命令也是**板端命令**。本文中“host 程序”指运行在 ARM 上的 `app_host`，不要与“开发主机 Ubuntu”混淆。

| 环境 | qmake 在哪里运行 | 产物在哪里运行 | 用途 |
|---|---|---|---|
| Ubuntu 桌面 Qt5 | Ubuntu20 | Ubuntu20，通常是 x86-64 ELF | 界面、参数及 Linux IPC 调试 |
| ARM Qt SDK | Ubuntu20 | DM8168，32 位 ARM ELF | 正式板端 GUI |
| 板端 Qt 运行库 | 不要求板上有 qmake | DM8168 | 加载 GUI 所需库、插件、字体 |

**ARM Qt SDK 中的 qmake 本身通常是 x86 Linux 程序，这是正常的。要检查的是它选择的编译器、目标 Qt 库以及最终 ARM 产物。** 仅安装 `qtcreator` 或 `qtbase5-dev` 不会得到 DM8168 的 ARM Qt SDK。

建议执行顺序：板端信息采集 → Ubuntu 源码与桌面测试 → 识别 ARM SDK → host/DSP 构建 → ARM Qt 构建 → 部署 → Qt/IPC 验收 → 显示与 DSP 共存验收。

## 2. 板端：先采集环境，不改启动配置

有设备时连接 HDMI 和 USB 鼠标，使用已有方式启动板子。当前无显示设备也可以通过串口/现有远程终端完成以下信息采集。日志保存在 `/tmp`，重启前需导出。

```sh
# [板端 Linux]
BOARD_LOG=/tmp/ex04-env-$(date +%Y%m%d-%H%M%S)-$$
mkdir -p "$BOARD_LOG"
printf 'BOARD_LOG=%s\n' "$BOARD_LOG"

uname -a > "$BOARD_LOG/uname.txt"
cat /proc/cmdline > "$BOARD_LOG/cmdline.txt"
cat /proc/iomem > "$BOARD_LOG/iomem.txt"
cat /proc/meminfo > "$BOARD_LOG/meminfo.txt"
cat /proc/modules > "$BOARD_LOG/modules.txt"
cat /proc/fb > "$BOARD_LOG/fb.txt"
cat /proc/bus/input/devices > "$BOARD_LOG/input.txt"
mount > "$BOARD_LOG/mount.txt"
ps > "$BOARD_LOG/ps.txt"
dmesg > "$BOARD_LOG/dmesg.txt"
ls -l /dev/fb* /dev/input/* > "$BOARD_LOG/devices.txt" 2>&1
if [ -f /etc/init.d/load-hd-firmware.sh ]; then
    cat /etc/init.d/load-hd-firmware.sh > "$BOARD_LOG/display-startup.txt"
fi
find /usr /opt /lib \( -name 'libQtCore.so*' -o -name 'libQt5Core.so*' \
    -o -name 'libqlinuxfb.so*' -o -name 'qmake' \) \
    > "$BOARD_LOG/qt-files.txt" 2> "$BOARD_LOG/qt-find-errors.txt"

cat "$BOARD_LOG/uname.txt" "$BOARD_LOG/cmdline.txt"
cat "$BOARD_LOG/fb.txt" "$BOARD_LOG/qt-files.txt"
```

若 Qt 位于其他已知挂载目录，对那个目录另执行 `find`。库文件名只是线索：

| 发现 | 下一步 |
|---|---|
| `libQtCore.so.4...` | 优先核对课程原有 Qt4/Embedded SDK、版本与 QWS；不是任何 Qt4 库都支持 QWS |
| `libQt5Core.so.5...` 和 LinuxFB 插件 | 核对配套 Qt5 ARM SDK，以及插件的依赖与实际显示能力 |
| 两套都有 | 先沿用板上已经跑通的例程及其启动环境，选择一套 SDK；不要混合库和插件 |
| 都没找到 | 检查原 Qt 例程和启动脚本；可能尚未部署运行库，也可能在其他目录或采用静态链接 |

从 `/proc/cmdline`、`mount` 找出根文件系统来源。若是 NFS，记录服务器 IP 和导出路径；若从 SD 卡/本地存储启动，后面的 NFS 复制步骤不直接适用，应使用已有的板端传输方法。

**本步通过：** 能确定板端系统、显示设备、输入设备、Qt 线索及 rootfs 来源。找不到 Qt SDK 时仍可完成第 3～4 节桌面测试，正式交叉编译留到 SDK 确认后。

## 3. Windows → Ubuntu20：准备当前源码副本

### 3.1 Windows：核对目录

```powershell
# [Windows PowerShell]
Set-Location 'D:\develop\vscode\projects\ex04'
Get-ChildItem frontend_arm
Get-ChildItem PPT
git status --short
```

应能看到 `frontend_arm/frontend_arm.pro`、`products.mak`、`host/`、`dsp/`。本次新增文件可能尚未提交，**仅重新 clone 仓库不保证包含当前工作区改动**。

优先使用已有的虚拟机共享文件夹。若没有共享文件夹，可先在 Windows 打包当前源码，再通过已有的文件传输通道把压缩包送到 Ubuntu：

```powershell
# [Windows PowerShell；可选，与共享目录方式二选一]
$SourceArchive = Join-Path $env:TEMP 'ex04-arm-source.tar.gz'
tar -czf $SourceArchive --exclude=.git --exclude=.codex --exclude=.agents --exclude='frontend_arm/build' --exclude='frontend_arm/build-*' --exclude=host/bin --exclude=dsp/bin --exclude=dsp/configuro --exclude=install --exclude=stage-arm .
if ($LASTEXITCODE -ne 0) { throw '源码打包失败，请查看输出。' }
Get-Item -LiteralPath $SourceArchive
```

无需为了传文件重装板子或修改启动方式。

### 3.2 Ubuntu20：安装桌面开发及诊断工具

在**未 source 任何交叉编译 SDK** 的普通 Ubuntu 终端执行：

```bash
# [Ubuntu20 Bash]
cat /etc/os-release
uname -m
sudo apt update
sudo apt install build-essential qtbase5-dev qt5-qmake qtbase5-dev-tools \
    python3 file binutils rsync ripgrep
```

每条成功后再执行下一条。若 `apt update` 失败，先处理虚拟机网络或当前软件源，不直接改成不明来源的旧软件源。这里安装的是 Ubuntu 本机工具；不包含 TI DSP 编译器、SysLink SDK 或 ARM Qt 运行库。

将源码复制到 Ubuntu 自身文件系统中构建，避免在 Windows 共享目录里产生权限、软链接和生成文件问题：

```bash
# [Ubuntu20；共享目录方式]
SOURCE_SHARE='/实际共享目录/ex04'
PROJECT_ROOT="$HOME/ex04-arm-work"
test -f "$SOURCE_SHARE/frontend_arm/frontend_arm.pro" || exit 1
mkdir -p "$PROJECT_ROOT"
rsync -a --exclude='.git/' --exclude='.codex/' --exclude='.agents/' \
    --exclude='frontend_arm/build*/' --exclude='host/bin/' --exclude='dsp/bin/' \
    --exclude='dsp/configuro/' --exclude='install/' --exclude='stage-arm/' \
    "$SOURCE_SHARE/" "$PROJECT_ROOT/"
cd "$PROJECT_ROOT" || exit 1
test -f frontend_arm/frontend_arm.pro
```

`SOURCE_SHARE` 必须替换为 Ubuntu 能访问的路径，例如实际的 VMware 共享目录；不要填写 `D:\...`。此复制命令没有 `--delete`。第一次使用新工作目录；若后续 SDK 发生变化，应使用新的构建目录。

若采用压缩包方式，用以下命令代替上面的 `rsync`：

```bash
# [Ubuntu20；压缩包方式]
PROJECT_ROOT="$HOME/ex04-arm-work"
SOURCE_ARCHIVE='/实际接收目录/ex04-arm-source.tar.gz'
test -f "$SOURCE_ARCHIVE" || exit 1
mkdir -p "$PROJECT_ROOT"
tar -xzf "$SOURCE_ARCHIVE" -C "$PROJECT_ROOT"
cd "$PROJECT_ROOT" || exit 1
test -f frontend_arm/frontend_arm.pro
```

后续操作针对这个 Ubuntu 副本。修改 Windows 源码后需重新同步；不要覆盖 Ubuntu 中已经调整好的 `products.mak` 而不检查差异。

## 4. Ubuntu20：先完成桌面 Qt5 测试

这一阶段不需要板端 SDK，可先验证界面和 Linux IPC。沿用第 3 节的普通终端：

```bash
# [Ubuntu20；桌面环境，未 source ARM SDK]
PROJECT_ROOT="$HOME/ex04-arm-work"
cd "$PROJECT_ROOT" || exit 1
/usr/bin/qmake -v
BUILD_TARGET=desktop BUILD_DIR="$PROJECT_ROOT/frontend_arm/build-desktop" \
    sh frontend_arm/build.sh /usr/bin/qmake
```

成功后单独执行：

```bash
# [Ubuntu20]
file frontend_arm/build-desktop/bin/ex04_frontend_arm
EX04_IPC_PEER="$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_ipc_peer" \
    python3 frontend_arm/tests/test_ipc_peer.py
CC=gcc python3 frontend_arm/tests/test_host_stop.py

mkdir -p "$PROJECT_ROOT/frontend_arm/build-desktop/tests"
cd "$PROJECT_ROOT/frontend_arm/build-desktop/tests" || exit 1
/usr/bin/qmake "$PROJECT_ROOT/frontend_arm/tests/tests.pro"
make -j2
EX04_IPC_PEER="$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_ipc_peer" \
    ./test_frontend_arm
```

最后在 Ubuntu 图形桌面的终端中运行：

```bash
# [Ubuntu20 图形桌面]
PROJECT_ROOT="$HOME/ex04-arm-work"
"$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_frontend_arm" --windowed
```

**通过标准：** 桌面构建为本机架构；C IPC 测试 6 项、host 停止回归测试 5 项及 Qt 集成测试通过；界面可打开；两项 IPC 验证各收到 10 条消息，完成后界面仍响应。桌面阶段只操作“IPC 验证”，不在这里运行板端音频业务。

桌面测试通过不能代替 ARM 构建、板端 Qt4/Qt5 兼容性或真实 SysLink/ALSA 验证。

## 5. Ubuntu20：找到并确认 ARM SDK

### 5.1 先找已有 SDK

开一个**新的普通 Ubuntu 终端**执行：

```bash
# [Ubuntu20]
find "$HOME" /opt \( -name 'environment-setup*' -o -name 'qmake' \
    -o -name 'arm-none-linux-gnueabi-gcc' -o -name 'cl6x' \) \
    -print 2>/dev/null
```

SDK 若安装在其他已知磁盘，对该目录再搜索。工程 `products.mak` 中的 `/home/user/...` 是示例路径，不是已确认存在于你的虚拟机中的路径。

需要凑齐的环境如下：

| 构建对象 | 必须具备 |
|---|---|
| `app_host` | 配套 ARM GCC、目标 sysroot、ARM ALSA 头文件和库、匹配的 SysLink 库与头文件 |
| `server_dsp.xe674` | TI C674 ELF 编译器、XDCtools、SYS/BIOS、IPC、SysLink |
| ARM Qt GUI | 对应板端的 Qt 版本、宿主机 qmake/moc/uic、ARM Qt 头文件/库/mkspec、交叉编译器和 sysroot |
| 板端运行 | 兼容的动态加载器/libc/libstdc++、Qt 库、显示/输入后端、字体、匹配内核的 SysLink 环境 |

优先复用已经能运行该 DM8168 镜像的课程 SDK/BSP。不要直接安装 Ubuntu 的 `gcc-arm-linux-gnueabihf` 就替换原工具链；当前工程默认前缀是 `arm-none-linux-gnueabi-`，实际浮点 ABI、libc 和目标根文件系统必须匹配。

TI EZSDK 5.05 指南提供了 `linux-devkit/environment-setup` 的交叉编译入口及 Qt/Embedded 开发包；它原先支持的开发发行版是 Ubuntu10.04/11.10，**不能据此保证老二进制工具在 Ubuntu20 开箱即用**。本报告保留 Ubuntu20，遇到老工具无法启动时按第 11 节诊断，不先重装系统。[TI EZSDK 5.05 开发指南，第 2、15～18 页](https://software-dl.ti.com/dsps/dsps_public_sw/ezsdk/5_05_01_04/exports/DM816x_EZ_Software_Developers_Guide.pdf)

### 5.2 选择 Qt4 或 Qt5 路线

| 板端已经确认的环境 | 本工程路线 | 需要注意 |
|---|---|---|
| Qt4/Embedded，课程 QWS 例程可运行 | 匹配的 Qt4 ARM SDK；`run-qt4.sh` | 启动参数为 `-qws` |
| Qt5，LinuxFB 例程可运行 | 匹配的 Qt5 ARM SDK；`run-qt5.sh` | 需要可用的 LinuxFB 后端及其依赖 |
| 无可用 Qt，或找不到配套 SDK | 先取得课程/板厂的匹配 SDK 与运行库 | 先完成信息采集和桌面测试，不猜一套 ARM 库复制进去 |

Qt4 的交叉编译需要目标 mkspec 配合 ARM 工具链；具体配置取决于板级 SDK。[Qt 官方 Wiki：Qt4 Embedded 构建](https://wiki.qt.io/Building_Qt_for_Embedded_Linux)

Qt5 已取消 QWS，使用平台插件；LinuxFB 通过 framebuffer 绘制软件界面。本文不要求加入 EGLFS/SGX。[Qt5 官方嵌入式 Linux 文档](https://doc.qt.io/archives/qt-5.15/embedded-linux.html)

若已有匹配 EZSDK 的 Qt4，可在**专用 Qt 交叉编译终端**中检查：

```bash
# [Ubuntu20；专用 Qt 交叉编译终端，路径先替换]
EZSDK='/实际路径/ti-ezsdk_dm816x-evm_5_05_01_04'
test -f "$EZSDK/linux-devkit/environment-setup" || exit 1
source "$EZSDK/linux-devkit/environment-setup"
command -v qmake
qmake -v
qmake -query
```

记录这里的 qmake 绝对路径，并检查是否属于该 SDK。若仍指向 `/usr/bin/qmake`，说明没有正确选中 SDK；先修正环境。Qt5 则使用板厂/课程 Qt5 SDK 对应的环境脚本，不照搬 Qt4 路径。SDK 移动过目录时，还要检查环境脚本和 mkspec 中的旧绝对路径。

**没有匹配 SDK 的处理：** 向课程资料/BSP 中查找“主机交叉开发包 + 对应板端运行包 + 构建配置”。若只能拿到 Qt 源码，需要先明确板端 libc/浮点 ABI、目标 sysroot、编译器、Qt 版本、显示/输入配置，再生成 SDK。当前这些事实未齐，本文不提供看似可复制、实际可能生成错误 ABI 的通用 `configure` 命令。板端只有 `.so` 文件也不等于具备可用的开发 SDK。

### 5.3 记录本机路径，供后续终端复用

把以下内容保存为 Ubuntu 的 `~/ex04-arm-env.sh`，**将所有“实际路径”替换完再使用**。它只保存路径，不加载 SDK：

```bash
# [Ubuntu20；文件 ~/ex04-arm-env.sh 的内容]
export PROJECT_ROOT="$HOME/ex04-arm-work"
export EZSDK='/实际路径/ti-ezsdk_dm816x-evm_5_05_01_04'
export ARM_PREFIX='/实际路径/arm-2014.05/bin/arm-none-linux-gnueabi-'
export ARM_SYSROOT="$EZSDK/linux-devkit/arm-none-linux-gnueabi"
export QMAKE_ARM='/实际路径/ARM-Qt-SDK/bin/qmake'
export QT_MAJOR=4
export QT_BUILD="$PROJECT_ROOT/frontend_arm/build-arm-qt$QT_MAJOR"
export NFS_ROOT='/从板端启动配置核实的NFS导出根目录'
```

选择 Qt5 时将 `QT_MAJOR=5`。如果不是 NFS 启动，暂不使用 `NFS_ROOT`，按第 8.3 节传输。不要让桌面、Qt4 ARM、Qt5 ARM 共用一个构建目录；同一 Qt 主版本更换 SDK 时也换目录。

## 6. Ubuntu20：重新构建现有 ARM host 和 DSP

打开一个**新的普通终端**，不要 source Qt 的 SDK 环境脚本；本工程的 makefile 用 `products.mak` 和下面的参数选择工具链。

```bash
# [Ubuntu20；host/DSP 构建终端]
source "$HOME/ex04-arm-env.sh"
cd "$PROJECT_ROOT" || exit 1
make .show DEPOT="$EZSDK" CGT_ARM_PREFIX="$ARM_PREFIX" ARM_SYSROOT="$ARM_SYSROOT"
file -L "${ARM_PREFIX}gcc"
"${ARM_PREFIX}gcc" --version
"${ARM_PREFIX}gcc" -dumpmachine
"${ARM_PREFIX}gcc" -print-sysroot
test -f "$ARM_SYSROOT/usr/include/alsa/asoundlib.h"
```

逐项核对 `.show` 输出目录是否存在。`-print-sysroot` 显示编译器默认值，本工程构建时显式传入的 `ARM_SYSROOT` 可以不同，最终以编译命令中的 `--sysroot=` 为准。

`products.mak` 默认组件为 BIOS `6_33_05_46`、IPC `1_24_03_32`、SysLink `2_20_00_14`、XDCtools `3_23_03_53`、C674 工具链 `7_3_1`。若实际安装位置不同，在 Ubuntu 副本中编辑对应路径；不要仅凭目录名称相近就混搭组件版本。

确认路径正确后：

```bash
# [Ubuntu20]
mkdir -p "$PROJECT_ROOT/build-logs"
make DEPOT="$EZSDK" CGT_ARM_PREFIX="$ARM_PREFIX" ARM_SYSROOT="$ARM_SYSROOT" \
    > "$PROJECT_ROOT/build-logs/host-dsp.log" 2>&1
BUILD_RC=$?
tail -n 60 "$PROJECT_ROOT/build-logs/host-dsp.log"
printf 'BUILD_RC=%s\n' "$BUILD_RC"
test "$BUILD_RC" -eq 0 || exit 1

file host/bin/release/app_host dsp/bin/release/server_dsp.xe674
readelf -l host/bin/release/app_host
readelf -A host/bin/release/app_host
readelf -d host/bin/release/app_host
```

**通过标准：** make 返回 0，debug/release 产物齐全，`app_host` 为 ARM，DSP 文件为 TI C674 目标产物。不要要求 `server_dsp.xe674` 是 ARM ELF。

本次 `host/app/main.c` 已加入 `main: control ready` 就绪输出及协作停止处理，**必须把新构建的 `app_host` 与 GUI 一起部署**。只有更换 GUI、继续用旧 host，会影响停止行为。

若缺 ARM ALSA 头文件或 `-lasound`，应补齐匹配目标 sysroot 的 ALSA 开发文件；在 Ubuntu 安装本机 `libasound2-dev` 不能解决 ARM 链接问题。

## 7. Ubuntu20：交叉编译 ARM Qt GUI 和显示探针

回到第 5.2 节的**专用 Qt 交叉编译终端**，先加载该 SDK 必需的环境，再加载路径记录。Qt4 EZSDK 的示例如下；Qt5 用自己的 SDK 环境入口替换 `source` 那一行：

```bash
# [Ubuntu20；Qt4 EZSDK 示例]
source "$HOME/ex04-arm-env.sh"
source "$EZSDK/linux-devkit/environment-setup"
source "$HOME/ex04-arm-env.sh"
cd "$PROJECT_ROOT" || exit 1
file -L "$QMAKE_ARM"
"$QMAKE_ARM" -v
"$QMAKE_ARM" -query
BUILD_DIR="$QT_BUILD" sh frontend_arm/build.sh "$QMAKE_ARM"
```

构建成功后检查目标配置与产物：

```bash
# [Ubuntu20；同一 Qt SDK 环境]
rg -n '^(CC|CXX|LINK|CFLAGS|CXXFLAGS|LFLAGS|LIBS)[[:space:]]*=' \
    "$QT_BUILD/app/Makefile" "$QT_BUILD/ipc_peer/Makefile"
file "$QT_BUILD/bin/ex04_frontend_arm" "$QT_BUILD/bin/ex04_ipc_peer"
readelf -l "$QT_BUILD/bin/ex04_frontend_arm"
readelf -A "$QT_BUILD/bin/ex04_frontend_arm"
readelf -d "$QT_BUILD/bin/ex04_frontend_arm"

mkdir -p "$QT_BUILD/probe"
cd "$QT_BUILD/probe" || exit 1
"$QMAKE_ARM" "$PROJECT_ROOT/tests/qt_syslink_probe/qt_syslink_probe.pro"
make -j2
file qt_syslink_probe
```

`build.sh` 默认会拒绝非 ARM 产物。继续核对 Makefile 中编译器和 Qt 库确实指向 ARM SDK，并对实际链接到的 Qt 库使用 `file -L` 检查架构。`qmake -query` 中有些目录是目标板安装路径，不能假定该路径就是 Ubuntu 上可直接复制的运行库目录。

**通过标准：** GUI、IPC peer、probe 都是 32 位 ARM；ELF 动态加载器路径、浮点 ABI、Qt 及 C++ 库需求与目标 rootfs 相容。ARM 架构检查通过只是第一关；最终还需板端加载和运行验证。

## 8. Ubuntu20：先暂存，再部署到板端可见目录

### 8.1 暂存完整业务目录

回到第 6 节的 host/DSP 普通终端：

```bash
# [Ubuntu20；host/DSP 普通终端]
source "$HOME/ex04-arm-env.sh"
cd "$PROJECT_ROOT" || exit 1
STAGE_ROOT="$PROJECT_ROOT/stage-arm"
make install EXEC_DIR="$STAGE_ROOT" DEPOT="$EZSDK" \
    CGT_ARM_PREFIX="$ARM_PREFIX" ARM_SYSROOT="$ARM_SYSROOT"
```

成功后继续：

```bash
# [Ubuntu20]
STAGE_APP="$STAGE_ROOT/ex04_syslink_dsp/release"
BUILD_DIR="$QT_BUILD" sh frontend_arm/install.sh "$STAGE_APP"
install -m 755 "$QT_BUILD/probe/qt_syslink_probe" "$STAGE_APP/"
chmod 755 "$STAGE_APP/app_host" "$STAGE_APP/slaveloader" "$STAGE_APP/run.sh"
ls -l "$STAGE_APP"
file "$STAGE_APP/ex04_frontend_arm" "$STAGE_APP/ex04_ipc_peer" \
    "$STAGE_APP/app_host" "$STAGE_APP/slaveloader"
```

注意两个安装入口的路径语义不同：

| 入口 | 参数填写什么 | 最终位置 |
|---|---|---|
| 根目录 `make install EXEC_DIR=...` | 暂存父目录 | 自动追加 `ex04_syslink_dsp/debug` 和 `release` |
| `frontend_arm/install.sh ...` | 最终业务目录 | 直接安装到传入目录 |

本工程安装脚本不安装 Qt 库、平台插件、字体，也不配置内核模块。先确认暂存目录包含 GUI、peer、probe、`app_host`、`slaveloader`、`server_dsp.xe674` 和启动脚本。

### 8.2 使用现有 NFS rootfs 的部署方式

先核对路径映射：

| Ubuntu20 中 | DM8168 中 |
|---|---|
| `$NFS_ROOT` | `/` |
| `$NFS_ROOT/home/root/ex04_syslink_dsp/release` | `/home/root/ex04_syslink_dsp/release` |

Ubuntu 可用 `sudo exportfs -v` 检查现有导出；如果没有这个命令，先确认 NFS 服务是否其实位于另一台机器。**只有板端正在使用的根文件系统确实对应这里的导出目录，才能使用下列步骤。** 不根据示例猜测 `/home/用户名/targetfs`。

部署前在板端正常退出 GUI，确认音频任务结束、DSP 已正常 shutdown、没有 CLI 正在执行相同业务。不能一边运行一边替换二进制。

```bash
# [Ubuntu20；已核实 NFS_ROOT]
source "$HOME/ex04-arm-env.sh"
STAGE_ROOT="$PROJECT_ROOT/stage-arm"
DEPLOY_ROOT="$NFS_ROOT/home/root/ex04_syslink_dsp"
test -d "$NFS_ROOT/etc" || exit 1
test -f "$STAGE_ROOT/ex04_syslink_dsp/release/ex04_frontend_arm" || exit 1
DEPLOY_TAG=$(date +%Y%m%d-%H%M%S)-$$
if [ -d "$DEPLOY_ROOT" ]; then
    sudo cp -a "$DEPLOY_ROOT" "$DEPLOY_ROOT.backup-$DEPLOY_TAG" || exit 1
fi
sudo mkdir -p "$DEPLOY_ROOT"
sudo cp -a "$STAGE_ROOT/ex04_syslink_dsp/." "$DEPLOY_ROOT/" || exit 1
printf '%s\n' "$DEPLOY_TAG" | sudo tee "$DEPLOY_ROOT/release/DEPLOYMENT_ID.txt"
```

随后在板端确认看到同一个部署编号：

```sh
# [板端]
APPDIR=/home/root/ex04_syslink_dsp/release
cd "$APPDIR" || exit 1
cat DEPLOYMENT_ID.txt
ls -l ex04_frontend_arm ex04_ipc_peer qt_syslink_probe app_host slaveloader server_dsp.xe674
```

编号不一致时先查 NFS 映射，不继续启动。本文保留旧目录备份，不删除 WAV 或其他已有数据。

### 8.3 不是 NFS rootfs 的情况

在 Ubuntu 打包暂存目录，然后使用已经可用的 SCP、共享挂载或移动存储传到板端：

```bash
# [Ubuntu20]
tar -czf "$PROJECT_ROOT/ex04-arm-deploy.tar.gz" \
    -C "$PROJECT_ROOT/stage-arm" ex04_syslink_dsp
```

如果板端已经启用 SSH/SCP，可使用 `scp "$PROJECT_ROOT/ex04-arm-deploy.tar.gz" root@实际板端IP:/tmp/`；没有 SSH 时用现有通道，不假设板端支持该命令。传输完成后：

```sh
# [板端；正常结束现有业务后]
mkdir -p /home/root
if [ -d /home/root/ex04_syslink_dsp ]; then
    cp -a /home/root/ex04_syslink_dsp \
        /home/root/ex04_syslink_dsp.backup-$(date +%Y%m%d-%H%M%S)-$$ || exit 1
fi
tar -xzf /tmp/ex04-arm-deploy.tar.gz -C /home/root
ls -l /home/root/ex04_syslink_dsp/release
```

如果实际接收目录不是 `/tmp`，相应替换解压路径。检查存储空间足够后操作。

## 9. 板端：配置运行库，先验收 Qt 与 Linux IPC

### 9.1 检查运行依赖

```sh
# [板端]
APPDIR=/home/root/ex04_syslink_dsp/release
cd "$APPDIR" || exit 1
if command -v ldd >/dev/null 2>&1; then
    ldd ./ex04_frontend_arm
    ldd ./ex04_ipc_peer
    ldd ./app_host
    ldd ./slaveloader
fi
```

出现 `not found` 先解决对应目标库。板上没有 `ldd` 时，用 Ubuntu 的 `readelf -l/-d` 检查依赖，并逐项核对板端库和动态加载器；这仍需实际启动来验证。不要复制 Ubuntu 的 `/usr/lib/x86_64-linux-gnu` 到板端。

若板上原有 Qt 例程已经能运行，先沿用它的运行环境。只有库位于非默认目录时才设置路径；下面的 `QT_RUNTIME` 是**板端目录**，必须替换：

```sh
# [板端；Qt4/Qt5 共用的非默认库目录示例]
QT_RUNTIME=/实际板端Qt安装目录
export LD_LIBRARY_PATH="$QT_RUNTIME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

Qt4/Embedded 可按 SDK 原配置指定 `QTDIR` 和字体目录。若 Qt5 在系统默认环境中已经能找到插件和字体，跳过下面的路径覆盖；需要指定非默认位置时，先填写实际板端 Qt 根目录，再按实际布局设置：

```sh
# [板端；仅 Qt5，先确认这些目录实际存在]
QT_RUNTIME=/实际板端Qt安装目录
export QT_QPA_PLATFORM_PLUGIN_PATH="$QT_RUNTIME/plugins/platforms"
export QT_QPA_FONTDIR="$QT_RUNTIME/lib/fonts"
export QT_QPA_PLATFORM=linuxfb
```

如果 SDK 插件或字体另放在其他目录，将相应变量改为实际目录，不存在的示例目录不要照填。中文界面需要目标 Qt 能加载的中文字库，不能仅凭有一款英文字体就认定中文正常。可使用课程 SDK 已验证的字体并遵循其许可证。

若板端尚无运行库，应从**同一 ARM Qt SDK 的目标安装/暂存目录**部署完整匹配运行包，保留软链接和插件目录结构；再按实际路径设置环境。不要从 qmake 的安装路径盲目复制整套宿主机 SDK，也不要为消除依赖错误覆盖板端系统 libc。静态 Qt 的插件处理以该 SDK 的构建方式为准。

Qt5 默认使用 `/dev/fb0`；只有确认 HDMI 对应其他设备，才设置例如 `QT_QPA_PLATFORM=linuxfb:fb=/dev/fb1`。输入设备先依据 `/proc/bus/input/devices` 识别，不把其他板子的 `event0` 写死。[Qt5 LinuxFB 参数说明](https://doc.qt.io/archives/qt-5.15/embedded-linux.html#linuxfb)

### 9.2 退出原桌面，保留显示固件

```sh
# [板端；仅在存在该 Matrix 服务时]
if [ -x /etc/init.d/matrix-gui-e ]; then
    /etc/init.d/matrix-gui-e stop
fi
```

脚本不存在时按原系统方式退出占用显示的 GUI。**不执行 `/etc/init.d/load-hd-firmware.sh stop`，不使用旧 frontend 的 Fix Issues**；那会影响显示固件。尚未确认 DSP 内存与固件关系时，此处只运行 Qt 和独立 Linux IPC。

### 9.3 启动 GUI：以下两条只选匹配的一条

以下启动命令应在已完成第 9.1 节环境设置的板端会话中执行。每次重新登录或打开新会话，先重新执行适用的 Qt 库、插件、字体及显示环境设置；它们不会随 APPDIR/LOGDIR 自动恢复。

```sh
# [板端；每个新会话重新设置变量]
APPDIR=/home/root/ex04_syslink_dsp/release
LOGDIR=/tmp/ex04-qt-$(date +%Y%m%d-%H%M%S)-$$
mkdir -p "$LOGDIR"
cd "$APPDIR" || exit 1
printf 'LOGDIR=%s\n' "$LOGDIR"
```

```sh
# [板端；Qt4/Embedded]
sh ./run-qt4.sh > "$LOGDIR/gui.log" 2>&1
```

```sh
# [板端；Qt5 LinuxFB]
sh ./run-qt5.sh > "$LOGDIR/gui.log" 2>&1
```

前台命令持续运行是正常现象。保留该会话，在第二个板端会话中把 `LOGDIR` 设置为刚才打印的同一路径，用 `tail -f "$LOGDIR/gui.log"` 查看日志。第二个会话不会自动继承第一个会话的变量。

依次操作：

1. 看 HDMI 上是否有完整界面、中文是否正常、右上角时钟是否连续变化；观察至少 2 分钟。
2. 鼠标切换到“IPC 验证”，点击“信号量同步验证”，等待 10 条消息完成。
3. 点击“共享内存数据验证”，等待 10 条带序号消息完成。
4. 再运行一次，测试“停止验证”，随后可再次运行。
5. 点击“退出”正常关闭。暂不点击音频页的“开始运行”。

**通过标准：** 无黑屏/花屏/中文方框；鼠标有效；两项 IPC 完成；退出后辅助进程和本次 IPC 资源正常清理。可在前后用 `ipcs -s`、`ipcs -m` 比较资源，板端无该工具时结合 GUI 清理日志检查。

这里的信号量/共享内存是 Linux System V IPC，**不证明 SysLink DSP 共存已经通过**。GUI 屏幕日志只保留最近 1500 行，完整输出以重定向的 `gui.log` 为准。

## 10. 板端：确认显示与 DSP 共存后再启用音频

### 10.1 运行 DSP 前的必过项

本工程当前配置的物理区间为：

| 用途 | 半开区间 |
|---|---|
| SR0 | `[0x8E000000, 0x8E010000)` |
| SR1 | `[0x8E010000, 0x8F000000)` |
| DSP 程序区 | `[0x8F000000, 0x90000000)` |

对照实际 Linux 内存、VPSS/M3 固件、VRAM/framebuffer、CMEM、DSP 最终链接 map，以及 SharedRegion 的编号/owner/缓存配置，确认兼容；确认 DSP 没有被开机业务占用。`/proc/iomem` 没列出的区间不一定空闲。信息不足时停在 Qt/IPC 阶段，不猜 `mem=128M`、`mem=169M` 或任意新地址。

完整核对表、失败定位和重复测试见 [Qt 与 SysLink 共存测试方案](qt-syslink-coexistence-test.md)。TI 原指南对默认 SysLink 示例有与 EZSDK 内存图不一致的说明，因此不能仅删除“停止显示固件”的命令就推断已经兼容。[TI EZSDK 开发指南，第 6 页](https://software-dl.ti.com/dsps/dsps_public_sw/ezsdk/5_05_01_04/exports/DM816x_EZ_Software_Developers_Guide.pdf)

### 10.2 用独立探针分阶段验收

先正常关闭业务 GUI，避免两个 QWS/LinuxFB 程序同时占用显示。板端会话 A 沿用已验证的 Qt 库和显示环境，选择对应命令：

```sh
# [板端会话 A；Qt4，Qt5 不执行这一条]
cd /home/root/ex04_syslink_dsp/release || exit 1
./qt_syslink_probe -qws > /tmp/ex04-probe.log 2>&1
```

```sh
# [板端会话 A；Qt5，Qt4 不执行这一条]
cd /home/root/ex04_syslink_dsp/release || exit 1
./qt_syslink_probe -platform "${QT_QPA_PLATFORM:-linuxfb}" > /tmp/ex04-probe.log 2>&1
```

探针正常刷新和点击至少 2 分钟后，会话 B 进行下列检查。**每个代码块执行后检查返回码和画面，再执行下一个；失败即停止后续试验。**

```sh
# [板端会话 B；已通过 10.1 的前提下]
APPDIR=/home/root/ex04_syslink_dsp/release
LOGDIR=/tmp/ex04-coexist-$(date +%Y%m%d-%H%M%S)-$$
mkdir -p "$LOGDIR"
cd "$APPDIR" || exit 1
cat /proc/modules
```

若 SysLink 已加载则复用；仅在确认既未加载也非内建时，用原系统配套模块执行 `modprobe syslink`，成功后继续。不要卸载现有显示相关模块。

```sh
# [板端会话 B：1. 加载 DSP]
./slaveloader startup DSP server_dsp.xe674 > "$LOGDIR/startup.log" 2>&1
START_RC=$?
printf 'START_RC=%s\n' "$START_RC"
cat "$LOGDIR/startup.log"
```

返回 0，观察画面和输入约 10 秒正常，继续：

```sh
# [板端会话 B：2. 单个 host 会话]
./app_host DSP smoke passthrough > "$LOGDIR/host.log" 2>&1
HOST_RC=$?
printf 'HOST_RC=%s\n' "$HOST_RC"
cat "$LOGDIR/host.log"
```

返回 0、5 轮数据校验正确，并出现 `main: control ready` 和正常结束输出，继续：

```sh
# [板端会话 B：3. 卸载 DSP]
./slaveloader shutdown DSP > "$LOGDIR/shutdown.log" 2>&1
STOP_RC=$?
printf 'STOP_RC=%s\n' "$STOP_RC"
cat "$LOGDIR/shutdown.log"
dmesg > "$LOGDIR/dmesg-after.txt"
```

返回 0 后继续观察显示与点击 30 秒。日志仍有 `QT_HEARTBEAT` 但 HDMI 冻结，不算通过。失败或阻塞时保存两端日志及 dmesg，不重复 startup，不并发启动第二个 host；未知状态下不把 `kill -9` 当正常清理。

再按同样完整周期手动做 3 次 `smoke swap`。每次都要 startup → 一个 host → shutdown；当前 DSP 不是一次加载后可无限接受新 host 会话的服务。后续 100 次周期和长时间音频按共存方案执行。

### 10.3 切换回业务 GUI

正常关闭探针（Escape；无键盘时可在确认 probe PID 后仅向该探针发送 SIGTERM），按第 9.3 节启动 GUI。在界面中设置：

| 字段 | 首次建议 |
|---|---|
| 业务目录 | `/home/root/ex04_syslink_dsp/release` |
| 运行模式 | `smoke` |
| 处理算法 | `swap` |
| 输入/输出路径 | smoke 不需要填写 |

首次 smoke 成功后再测 `record` 5 秒、`file`、`audio` 10 秒。空间算法的输入需满足 44100 Hz / 双声道 / 16 bit；GUI 中填写的是板端路径。`file` 填输出路径时进行离线保存，输出留空时播放。`record` 不需要 DSP。

运行中用 GUI 的“停止”按钮，让程序按既定流程结束。若出现清理失败或 30 秒后仍未退出，保存日志诊断；不要同时用 CLI 操作 DSP。GUI 的锁只协调本 GUI 实例，不能阻止外部命令行抢占 DSP。

## 11. 常见问题：在哪一端处理

| 现象 | 处理端 | 排查与处理 |
|---|---|---|
| `qmake: command not found` | Ubuntu | 桌面阶段核对 apt 安装；ARM 阶段检查 SDK 环境和绝对路径，不能用桌面 qmake 顶替 |
| `Expected 32-bit ARM ELF` | Ubuntu | 选错 qmake/mkspec 或复用了桌面 build 目录；换正确 SDK 和新目录重编译 |
| 老 GCC、qmake、XDC 工具明明存在却不能执行 | Ubuntu | `file -L /实际工具`、`readelf -l /实际工具`、`ldd /实际工具` 检查宿主机架构与缺失加载器/库；确为 32 位 x86 工具时再按缺项补兼容依赖，不盲装整套旧库 |
| 找不到 ALSA 或链接格式不兼容 | Ubuntu | 检查 `ARM_SYSROOT`、实际编译/链接命令、ARM `libasound`；不链接 Ubuntu 本机库 |
| 脚本报 `^M`、`\r` 或坏解释器 | Ubuntu 源码副本 | `file frontend_arm/*.sh` 检查 CRLF；可对这些脚本执行 `sed -i 's/\r$//' frontend_arm/*.sh`。项目已提供 LF 属性，传输时仍需确认 |
| 板上 `Exec format error` | Ubuntu 构建端 | 对实际部署文件执行 `file`，确认不是 x86/AArch64；重新构建 32 位 ARM |
| ELF 文件存在却报 `not found` | 板端 + Ubuntu | Ubuntu `readelf -l` 看 ELF 解释器，核对板上同路径加载器；这是 ABI/运行环境问题，不是简单 chmod |
| `GLIBC_x.y not found`、`GLIBCXX_x.y not found` | Ubuntu SDK / 板端依赖 | 用兼容板端的 sysroot 和工具链重编译，核对目标 libstdc++；不覆盖系统 libc 试错 |
| 找不到 `linuxfb` 平台插件 | 板端 | 核对 Qt5 插件目录和插件依赖；临时 `QT_DEBUG_PLUGINS=1 sh ./run-qt5.sh` 获取诊断，插件必须与目标 Qt 配套 |
| Qt4 不认识 `-qws` | SDK 选择 | 核对是否 Qt4/Embedded QWS 构建，而不是桌面 Qt4/其他版本 |
| 黑屏但日志仍输出 | 板端 | 先核对 `/proc/fb`、HDMI 对应设备、原 GUI 占用、显示固件；DSP startup 才触发则重点检查内存和固件所有权 |
| 鼠标不动 | 板端 | 核对 `/proc/bus/input/devices` 与设备节点，再按该 Qt SDK 已编译的输入驱动配置；不猜 event 编号 |
| 中文方框 | 板端 | 检查中文字库、字体目录和 Qt 字体支持；英文字体不能覆盖中文 |
| IPC 创建返回 `ENOSYS` 或资源错误 | 板端 | 核对内核 System V IPC 支持、权限、限制和本次错误日志；不能用 TI SharedRegion 代替这两个示例 |
| GUI 等不到 host 就绪 | Ubuntu + 板端 | 重新构建部署此次 `app_host`，确认日志有 `main: control ready`，检查业务目录是否指向旧文件 |
| DSP 启动/结束时黑屏，或 shutdown 非零 | 板端/固件配置 | 按共存方案保存首个失败阶段；不关闭 VPSS 来“修复”，不继续循环运行 |
| IPC 在强制结束后残留 | 板端 | 确认 GUI/peer 已退出，根据日志中的本次 ID 执行 `ipcrm -s 实际semid`、`ipcrm -m 实际shmid`；不要清空其他进程的全部 IPC |

## 12. 最后应形成的环境记录与验收结果

把以下表格填入实际值，与构建日志、板端日志一同保存。`/tmp` 在重启后可能丢失；NFS 正常时可复制到板端 `/home/root/ex04-test-logs/`，再从 Ubuntu 的对应 NFS 目录取回。非 NFS 时使用已有传输通道导出。

| 项目 | 实际值/结果 |
|---|---|
| Ubuntu 版本和 CPU 架构 | 待填 |
| 工程副本路径、源码版本/改动 | 待填 |
| EZSDK、SysLink、BIOS、IPC、XDC、C674 工具链路径/版本 | 待填 |
| ARM GCC 前缀、版本、sysroot | 待填 |
| ARM qmake 路径、Qt 版本、mkspec | 待填 |
| 板端 libc、浮点 ABI、ELF 加载器与 Qt 库路径 | 待填 |
| rootfs 来源；NFS 导出到板端路径映射 | 待填 |
| framebuffer、输入设备、字体目录 | 待填 |
| Linux/VPSS/VRAM/CMEM/DSP/共享区兼容性依据 | 待填 |
| Ubuntu 桌面构建和测试 | 未在用户 Ubuntu20 执行 |
| ARM 构建、依赖与 ABI 验证 | 未执行 |
| 板端 Qt/IPC、显示与 DSP 共存、音频验收 | 未执行 |

已有验证记录见 [frontend_arm/VALIDATION.md](../frontend_arm/VALIDATION.md)：此前在本机 Ubuntu22.04 WSL 中通过了 C IPC 和模拟 host 停止测试，并做过脚本与静态语法检查；它既不是用户的 Ubuntu20 虚拟机，也不代表 Qt4/Qt5 完整构建或 DM8168 硬件测试已通过。

课程依据为 `PPT/` 中第 6 次课的 Qt4/Qt5 环境与运行、第 7 次课的基础 Qt 例程、第 8 次课的进程交互资料。本报告保留现有 SysLink；RPE、波形/频谱库及 GPU 图形栈不作为本次环境搭建的前置条件。
