# 当前 DM8168 环境的 Qt5 VNC 探针操作步骤

日期：2026-11-01。以下步骤依据用户提供的 Ubuntu20 和 DM8168 输出编写，尚未在该虚拟机或板端代为执行。

进度更新：用户已执行第 1～5 节。探针交叉编译、HTTP 下载、板端 Qt5 VNC 插件加载和初次 5900 监听已通过。随后 Windows 同一网卡/源地址的两次 TCP 测试先 True、后 False，ping 仍可达；服务稳定性尚未通过。第 6 节已调整为先捕获退出证据、再直接使用完整 VNC 客户端连接，避免用纯 TCP 短连接反复测试这套旧插件。证据见 [板端首次启动日志](evidence/qt-vnc-probe-2026-11-01/board-first-start.log)。

已知条件：Ubuntu 的 Qt5.9.7 qmake/moc/rcc/uic 为 x86-64 工具，Qt5Core 和 VNC 插件为 32 位 ARM；板端存在同版本运行库与 VNC 插件。目标 mkspec 为 `linux-TIarmv7-g++`，release 标志包含 Cortex-A8/NEON/softfp，编译器前缀为 `arm-none-linux-gnueabi-`。qmake 内嵌安装前缀仍是 `/home/qt_arm/qt5.9.7_install`，与 Ubuntu 文件实际位置不同。板端没有 ldd，后续通过 ELF 信息与实际启动日志验证依赖。

## 1. 将探针源码包复制到 Ubuntu

使用已有虚拟机共享目录、拖放或文件传输方式，将 [探针源码包](evidence/qt-vnc-probe-2026-11-01/qt-vnc-probe-source.tar.gz) 放到 Ubuntu 的：

```text
/home/user/qt-vnc-probe-source.tar.gz
```

包内只有现有 `tests/qt_syslink_probe/main.cpp` 和 `qt_syslink_probe.pro`；探针显示变化色块、时间和点击计数，不调用 DSP/ALSA。VNC 由目标 Qt 平台插件提供。

源码包 SHA256：`5fd165afb8ff927e458bd26bb1125ecd80d11b9301b33910b70d94dcb1e604ec`。

```bash
# [Ubuntu20]
test -f "$HOME/qt-vnc-probe-source.tar.gz" || exit 1
sha256sum "$HOME/qt-vnc-probe-source.tar.gz"
mkdir -p "$HOME/qt-vnc-probe/src"
tar -xzf "$HOME/qt-vnc-probe-source.tar.gz" -C "$HOME/qt-vnc-probe/src" || exit 1
```

也可以直接把工程 `tests/qt_syslink_probe/` 中这两个源文件复制到该 `src` 目录，不要求先传输整个业务工程。

## 2. Ubuntu：为原 SDK 前缀建立路径映射

在普通 Ubuntu Bash 中执行，不 source Qt4/EZSDK 环境脚本。这里创建一个指向已有 Qt5 安装的符号链接，让 qmake、模块配置及原有绝对路径可以找到同一套文件。**只在 Ubuntu 建立此映射。** 板端已经存在实际 Qt5 目录，不需要做这个操作。

```bash
# [Ubuntu20]
EZSDK=/home/user/ti-ezsdk_dm816x-evm_5_05_01_04
QT5_DIR="$EZSDK/filesystem/ezsdk-dm816x-evm-rootfs/home/qt_arm/qt5.9.7_install"
QT5_PREFIX=/home/qt_arm/qt5.9.7_install
test -f "$QT5_DIR/bin/qmake" || exit 1

sudo mkdir -p /home/qt_arm || exit 1
if [ ! -e "$QT5_PREFIX" ] && [ ! -L "$QT5_PREFIX" ]; then
    sudo ln -s "$QT5_DIR" "$QT5_PREFIX" || exit 1
fi
if [ "$(readlink -f "$QT5_PREFIX")" != "$(readlink -f "$QT5_DIR")" ]; then
    printf 'STOP: %s already points elsewhere; inspect it before continuing.\n' "$QT5_PREFIX"
    exit 1
fi

test -f "$QT5_PREFIX/include/QtWidgets/QApplication" || exit 1
test -f "$QT5_PREFIX/mkspecs/linux-TIarmv7-g++/qmake.conf" || exit 1
"$QT5_PREFIX/bin/qmake" -v
```

若打印 STOP，保留原目录，先执行 `ls -ld /home/qt_arm/qt5.9.7_install`、`readlink -f /home/qt_arm/qt5.9.7_install` 核对，不用强制覆盖命令替换。

qmake 输出仍显示 `/home/qt_arm/qt5.9.7_install/lib` 是预期的：这个路径现在可以通过符号链接访问实际 SDK。此方法不修改原 Qt 二进制或原 SDK 配置。qmake 的属性、目标 mkspec 与宿主机工具路径说明见 [Qt 官方 qmake 文档](https://doc.qt.io/archives/qt-5.15/qmake-environment-reference.html)。

## 3. Ubuntu：编译显示探针

接着在同一 Ubuntu 终端执行。若第 2 节失败，先解决失败，不进入编译。

```bash
# [Ubuntu20]
export PATH="/home/user/arm-2014.05/bin:$PATH"
arm-none-linux-gnueabi-g++ --version
arm-none-linux-gnueabi-g++ -print-sysroot

PROBE_SOURCE="$HOME/qt-vnc-probe/src"
PROBE_BUILD="$HOME/qt-vnc-probe/build-qt597-arm"
test -f "$PROBE_SOURCE/qt_syslink_probe.pro" || exit 1
mkdir -p "$PROBE_BUILD"
cd "$PROBE_BUILD" || exit 1

"$QT5_PREFIX/bin/qmake" -nocache -spec linux-TIarmv7-g++ \
    "$PROBE_SOURCE/qt_syslink_probe.pro" \
    CONFIG+=release CONFIG-=debug CONFIG-=debug_and_release || exit 1

make -j2 || exit 1
file ./qt_syslink_probe
arm-none-linux-gnueabi-readelf -l ./qt_syslink_probe
arm-none-linux-gnueabi-readelf -d ./qt_syslink_probe
```

通过条件：make 成功；产物为 32 位 ARM ELF；Qt 依赖为 Qt5，不能出现 Qt4。记录 ELF 解释器路径，板端启动失败时可据此核对加载器是否存在。qmake 的 `QT_SYSROOT` 为空时，编译器使用自身默认配置；不因为它为空而随意改用另一套系统 sysroot。

若编译失败，保存完整命令和第一处错误。常见分类：

| 错误 | 优先检查 |
|---|---|
| Unknown module(s)、缺 Qt 头文件 | 第 2 节路径映射、include 和 mkspecs/modules 是否齐全 |
| arm-none-linux-gnueabi-g++ 找不到 | PATH 与实际编译器安装 |
| 链接找不到 Qt 或某个依赖 | 实际链接命令、Qt 的 .prl 文件及编译器 sysroot；不混入 x86 库 |
| VFP 参数不一致 | 实际编译器、mkspec 和目标 Qt 浮点 ABI；当前目标配置为 softfp |
| 本机工具无法执行 | 保存该工具的 file/ldd 结果；qmake 已经确认可在此 Ubuntu 启动 |

## 4. Ubuntu → 板端：通过临时 HTTP 传输

因为 NFS 映射尚未确认、板端目前只确认有 Telnet，本步骤使用 Ubuntu Python HTTP 服务与板端 wget。只提供探针构建目录，不需要 SSH 服务。

```bash
# [Ubuntu20；编译成功后]
hostname -I
python3 -m http.server 8000 --bind 0.0.0.0 \
    --directory "$HOME/qt-vnc-probe/build-qt597-arm"
```

记下板端可访问的 Ubuntu IPv4 地址，服务器保持前台运行。不要把 `192.168.1.21` 当成 Ubuntu 地址，它是 DM8168 的地址。若有多个网卡，选择从板端可达的地址。

在板端执行，先替换 `UBUNTU_IP`：

```sh
# [DM8168 Linux]
UBUNTU_IP='替换为Ubuntu的实际IP'
mkdir -p /home/root/qt-vnc-probe
cd /home/root/qt-vnc-probe || exit 1
wget -O qt_syslink_probe "http://$UBUNTU_IP:8000/qt_syslink_probe" || exit 1
chmod 755 qt_syslink_probe
ls -l qt_syslink_probe
```

若 HTTP 不通，先核对地址、路由与 Ubuntu 防火墙。也可使用已经验证的文件传输通道把该二进制复制到同一板端目录；不要仅因两端都有 Qt5 目录就假定 NFS 映射相同。下载完成后可在 Ubuntu HTTP 服务所在终端按 Ctrl+C 结束服务。

## 5. 板端：启动 Qt5 VNC 探针

先检查 `netstat -lnt`，确认 5900 没有被现有服务占用。以下环境变量只作用于探针进程，显式选用 Qt5 库与插件。

```sh
# [DM8168 Linux]
cd /home/root/qt-vnc-probe || exit 1
QT5_DIR=/home/qt_arm/qt5.9.7_install
netstat -lnt

LD_LIBRARY_PATH="$QT5_DIR/lib" \
QT_PLUGIN_PATH="$QT5_DIR/plugins" \
QT_QPA_PLATFORM_PLUGIN_PATH="$QT5_DIR/plugins/platforms" \
QT_DEBUG_PLUGINS=1 \
./qt_syslink_probe -platform 'vnc:size=640x480:port=5900' \
    > /tmp/ex04-qt5-vnc-probe.log 2>&1 &
VNC_PROBE_PID=$!

sleep 2
tail -n 80 /tmp/ex04-qt5-vnc-probe.log
netstat -lnt
```

预期：没有加载失败；日志持续出现 `QT_HEARTBEAT`；实际监听 5900。板端没有 ldd 时，`QT_DEBUG_PLUGINS=1` 的日志可以指出平台插件及其依赖加载错误。

| 日志/现象 | 下一步 |
|---|---|
| 文件存在但执行报 not found | 对照 readelf 输出的 ELF 解释器，检查板端同路径加载器 |
| 缺 .so、GLIBC/GLIBCXX 版本错误 | 保存完整错误，核对该运行包/编译器与 rootfs 兼容性 |
| Could not load / cannot load vnc | 看前面的插件详细加载错误，不只截最后一句 |
| 字体目录错误或文字不显示 | 查找实际 TTF 字体目录，必要时为该进程设置 QT_QPA_FONTDIR；字体路径需真实存在 |
| 心跳存在但没有监听 | 检查 VNC 端口错误、插件启动日志和已有端口占用 |

此测试使用 VNC 虚拟显示，不要求 HDMI、framebuffer 或物理鼠标，不停止显示固件，不加载 DSP。

## 6. Windows：连接并验收

### 6.1 当前 Qt5.9.7 的短连接风险及退出证据

Qt 官方 v5.9.7 源码中，`clientCursor` 初始为 0；它在客户端发送光标编码能力后才创建。最后一个客户端断开时，server 调用 `disableClientCursor()`，该函数直接使用此指针，没有判空。纯 TCP 探测连接后立即关闭、未完成 VNC 握手，因而存在触发空指针崩溃的路径。当前 True → False 的现象与该路径高度吻合；目标板的退出码/堆栈尚未取得，因此这是有源码依据的原因判断，仍需现场证据确认。

来源：[Qt5.9.7 qvncscreen.h](https://github.com/qt/qtbase/blob/v5.9.7/src/plugins/platforms/vnc/qvncscreen.h)、[qvncscreen.cpp](https://github.com/qt/qtbase/blob/v5.9.7/src/plugins/platforms/vnc/qvncscreen.cpp)、[qvnc.cpp](https://github.com/qt/qtbase/blob/v5.9.7/src/plugins/platforms/vnc/qvnc.cpp)、[qvncclient.cpp](https://github.com/qt/qtbase/blob/v5.9.7/src/plugins/platforms/vnc/qvncclient.cpp)。上述官方源码已读取核对。

当前停止对该服务执行 `Test-NetConnection` 或其他只建立 TCP 后关闭的端口探测，改为本地 `netstat` 观察监听并直接使用 VNC 查看器。原先先做 TCP 探测的步骤在这套 Qt5.9.7 上不合适，已移除。

先保留原启动日志，并在板端查看：

```sh
# [DM8168]
ps | grep '[q]t_syslink_probe'
netstat -lnt
tail -n 60 /tmp/ex04-qt5-vnc-probe.log
dmesg | tail -n 40
```

若旧探针仍在运行，先分析其日志/监听状态，不启动第二个探针抢占端口。确认旧探针已退出、5900 未监听后，用前台方式重试并捕获退出码：

```sh
# [DM8168；整个括号块一起执行，保持此会话打开]
cd /home/root/qt-vnc-probe || exit 1
QT5_DIR=/home/qt_arm/qt5.9.7_install
(
    LD_LIBRARY_PATH="$QT5_DIR/lib" \
    QT_PLUGIN_PATH="$QT5_DIR/plugins" \
    QT_QPA_PLATFORM_PLUGIN_PATH="$QT5_DIR/plugins/platforms" \
    QT_DEBUG_PLUGINS=1 \
    QT_LOGGING_RULES='qt.qpa.vnc=true' \
    ./qt_syslink_probe -platform 'vnc:size=640x480:port=5900' \
        > /tmp/ex04-qt5-vnc-foreground.log 2>&1
    VNC_RETRY_RC=$?
    printf 'VNC_EXIT=%s\n' "$VNC_RETRY_RC"
    tail -n 60 /tmp/ex04-qt5-vnc-foreground.log
    dmesg | tail -n 40
)
```

命令保持前台运行、暂不返回提示符是正常现象，此时转到 Windows 直接连接 VNC；如果进程退出，括号中的后续命令会输出退出码及日志。139 通常对应 SIGSEGV，137 通常对应 SIGKILL，但仅凭退出码不能确定具体调用栈或断定 OOM，需结合内核日志/调试信息。

若需要长期修复，应在匹配的 Qt 源码/SDK 中为 `disableClientCursor()` 增加空指针保护，并重建、验证 VNC 插件；不能仅通过反复重启视为服务稳定性已解决。

### 6.2 使用完整 VNC 客户端连接

保持前台探针会话打开，用 VNC 客户端连接板端 IP `192.168.1.21`，端口 `5900`。TigerVNC 可填写 `192.168.1.21::5900`，双冒号表示明确的 TCP 端口；或使用显示号 `:0`，以具体客户端格式为准。连接期间不再插入纯 TCP 端口测试。

本次 TigerVNC 1.16.2 默认连接正式 GUI 时曾黑屏，板端日志为 `Cannot handle 0 bpp client`。固定 256 色曾显示，但后续再次黑屏，不能作为稳定方案。当前使用 Windows 本机兼容转发初始化像素格式，用户已确认全彩中文、波形和断开重连正常。执行：

```powershell
& 'D:\develop\vscode\projects\ex04\tools\deploy\start-vnc.cmd'
```

脚本只在本机回环地址监听 5901，再连接板端 5900，不改动板端 Qt 插件。转发已启动时，也可载入 [连接配置](../delivery/ex04-vnc-via-bridge.tigervnc)。要求本机有 Python 3；日志和详细操作见 [兼容转发说明](../tools/deploy/VNC_BRIDGE.md)。中文方块另按 [字库安装步骤](空间处理与Qt更新编译打包说明书_XFTP.md#94-安装中文字库并重启-gui) 处理。

验收：

1. 能看到变化色块、时间及 ticks。
2. 持续观察 2 分钟，画面继续更新。
3. 远程点击使 Mouse clicks 增加，板端心跳中的 clicks 也增加。
4. 在 VNC 画面中按 Escape，探针正常退出；重新启动后仍可连接。

客户端断开不等于正常结束应用；这套旧插件还可能因未完成握手的断开而崩溃。用 Escape 主动结束探针时，前台运行块会返回并打印退出码。第 5 节的后台启动方式可在原终端使用 `kill "$VNC_PROBE_PID"` 定向结束独立探针；这些操作不作为音频/DSP 业务的停止流程。

上述通过后，再编译部署同一 Qt5 SDK 对应的 `frontend_arm` 业务 GUI，使用现有 `run-qt5.sh` 的 `QT_QPA_PLATFORM` 覆盖接口。探针通过证明 Qt5 VNC 的 ARM 显示/远程输入可用，不代替 DSP、ALSA 或真实 HDMI 共存验收。
