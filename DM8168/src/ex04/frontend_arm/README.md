# EX04 ARM 板端 Qt

当前使用的 Qt 工程。在 Ubuntu20 开发虚拟机中交叉编译，程序运行于
DM8168 ARM Linux；当前已通过 VNC 显示中文及波形，HDMI/鼠标仍待验收。
旧桌面前端已从工作目录移除，历史实现保留在 Git 历史和清理备份中。

逐步环境配置、三端命令、部署及验收见
[ARM Qt 环境配置与操作报告](../docs/arm-qt-environment-setup-report.md)。
没有板端显示设备时，见 [无屏测试与 VNC 联调](../docs/arm-qt-headless-validation.md)。

当前更新、XFTP 上传、Qt5.9.7 ARM 编译及整套打包，按 [更新编译打包说明书](../docs/空间处理与Qt更新编译打包说明书_XFTP.md) 操作。

源码采用 Qt4.6/Qt5 共用的基础 Widgets API、传统信号槽和 C++98 写法。
板端版本未确定时不直接选择虚拟机桌面 Qt 的 qmake；Ubuntu20 的版本
不能说明板端 Qt 版本。先核对板上 Qt 库、现有例程及对应 ARM SDK。

## 目录与职责

```text
frontend_arm/
  frontend_arm.pro       qmake 子工程入口
  app/                  HDMI GUI、本地进程控制、IPC 接收线程
  ipc_peer/             独立 Linux C 进程和双方共用的数据定义
  tests/                Qt 集成测试及不依赖 Qt 的 C IPC 测试
  build.sh              参数化交叉编译及 ARM ELF 检查
  install.sh            安装到指定 NFS/rootfs 目录
  run-qt4.sh            Qt4/Embedded QWS 启动入口
  run-qt5.sh            Qt5 LinuxFB 启动入口
```

GUI 不链接 ALSA、TI SysLink 库，不直接访问 DSP 共享区。音频链路仍是
`app_host -> SysLink/Notify/SharedRegion -> DSP`。IPC 示例使用 Linux
System V 信号量/共享内存，与 TI SharedRegion 是不同机制。

参考 `PPT/` 第6次课的交叉编译和显示方式、第7次课的基础控件与资源考虑、
第8次课的独立 Linux 进程、信号量和共享内存交互要求。当前已加入真实 WAV
波形与动态演示；频谱和 RPE 不在本轮范围内。

## Ubuntu20 交叉编译

准备与板端运行库匹配的 **ARM Qt qmake、mkspec、交叉编译器、sysroot**。
Qt4 需要带 QWS 的 Embedded 构建；Qt5 需要板端可用的 LinuxFB 插件。
不混用桌面 Qt 库和 ARM 库，也不在本工程内自动重建 Qt SDK。

```sh
cd /path/to/ex04/frontend_arm
sh build.sh /actual/path/to/arm-qt/bin/qmake
```

也可设置 `QMAKE_ARM`。`BUILD_DIR` 默认为本目录的 `build`，必须对不同 Qt
SDK 使用不同构建目录；`JOBS` 默认 2，`MAKE` 默认 make。编译器及 sysroot
由 ARM Qt mkspec 配置，先用 `qmake -query` 和生成的 Makefile 核对。

```sh
BUILD_DIR="$PWD/build-qt4" sh build.sh /path/to/qt4-arm/bin/qmake
BUILD_DIR="$PWD/build-qt5" sh build.sh /path/to/qt5-arm/bin/qmake
file build-qt5/bin/ex04_frontend_arm build-qt5/bin/ex04_ipc_peer
arm-none-linux-gnueabi-readelf -h build-qt5/bin/ex04_frontend_arm
arm-none-linux-gnueabi-readelf -d build-qt5/bin/ex04_frontend_arm
```

`build.sh` 默认要求两个产物均为 32 位 ARM ELF；桌面 qmake 产生的 x86
程序不会被当作 ARM 构建成功。ABI、动态加载器和 Qt 库版本还需与目标
rootfs 匹配，单凭 ELF 架构检查不能证明可以上板运行。

现有顶层 makefile 继续独立构建 host/DSP。**本次 host 新增了就绪通知和
各模式的协作停止，须重新构建部署 `app_host`**；旧 host 没有就绪通知时，
界面会等待其自然结束，不冒险提前发送 SIGINT。

## 安装与板端启动

先按根 README 的原流程安装 `app_host`、`server_dsp.xe674` 和 `slaveloader`。
再将新 GUI 和 C 辅助程序安装到同一业务目录，例如：

```sh
# Ubuntu 虚拟机：填写该 rootfs 在虚拟机中的实际挂载路径。
cd /path/to/ex04/frontend_arm
sh install.sh /actual/nfs/rootfs/home/root/ex04_syslink_dsp/release
```

若编译时设置了 `BUILD_DIR`，安装时传入同一值。安装脚本只复制本项目的
两个程序及启动脚本，不复制桌面 Qt 库或修改板端启动配置。

**板端**：先确认显示固件、内存映射与 SysLink 兼容，按
[`../docs/qt-syslink-coexistence-test.md`](../docs/qt-syslink-coexistence-test.md)
完成基线和分阶段验证。课程第6次课指出默认配置可能因停止 VPSS 固件而
丢失显示；进程隔离并不能自行消除固件或内存冲突。

只退出占用屏幕的 Matrix GUI，不停止显示固件。脚本存在时可执行：

```sh
/etc/init.d/matrix-gui-e stop
cd /home/root/ex04_syslink_dsp/release
./run-qt4.sh > qt-arm.log 2>&1
# 或：板端已有与程序匹配的 Qt5 LinuxFB 环境时
./run-qt5.sh > qt-arm.log 2>&1
```

沿用板上已验证的 `LD_LIBRARY_PATH`、framebuffer、鼠标和字体环境；Qt5
插件路径可按安装位置设置 `QT_QPA_PLATFORM_PLUGIN_PATH`，必要时设置
`QT_QPA_FONTDIR`；Qt4 使用对应 SDK 的 `QTDIR`、字体及 `QWS_MOUSE_PROTO`
配置。源码使用中文界面，需提供支持中文的字体。按板上实际设备配置，
不硬编码不存在的输入设备或图形插件路径。

默认全屏。调试可加 `--windowed`，窗口为 800×480；最小界面尺寸为
800×480，较大屏幕扩展绘图区。采用蓝白配色、顶部四页导航和固定底部操作栏。
业务目录默认 `/home/root/ex04_syslink_dsp/release`，修改后保存在独立的
`ex04_frontend_arm` 设置中。WAV 路径指向运行 GUI 的本机文件系统：桌面调试
时为桌面文件，上板时为板端文件；相对路径以业务目录为基准。

## 界面与波形

每次启动默认进入**演示环境**，无需音频设备或 DSP。在设置页切换到板端环境
后，开始按钮才会调用现有处理进程。演示不会写入音频文件，也不执行 HRIR 卷积。

| 页面 | 操作 |
|---|---|
| 波形 | 输入／输出 L/R 四轨对比，输入 WAV、对比 WAV、演示信号、缩放、全览、暂停观察 |
| 参数 | 文件处理／实时处理／录音；音源、保存位置、时长、算法及方向 |
| 诊断 | 有界日志、保存日志、smoke 自检、信号量及共享内存验证 |
| 设置 | 演示／板端环境、工作目录 |

- 点击波形查看时间及真实采样值；滚轮或 `+ / −` 缩放，按住左键拖动平移。
- 输入与对比 WAV 独立读取，均要求 44.1 kHz、双声道 PCM16；以各文件起点对齐，
  长度不同保留真实尾部，不做自动时延校正。
- 导入真实文件后清除模拟信号。文件演示只移动时间游标；未导入对比文件时输出保持空白。
  板端成功保存结果后自动加载结果 WAV；实时输入／输出 PCM 尚未接入时保持空白。
- 动态演示每 50 ms 更新，保留最近 5 秒。暂停观察只冻结视图，不停止任务；
  缩放／拖动动态波形时自动冻结，继续观察返回最新数据。
- 文件由独立工作线程以 64 KiB 块读取，包络最多 4096 桶；新文件／视区请求淘汰旧结果。
  不随文件大小或运行时长累积完整 PCM。暂停动态观察时最多多保留一份固定窗口。
- 运行期间参数、环境与文件导入锁定，导航、波形查看与停止仍然可用。

## 音频控制

| 模式 | 功能与算法 |
|---|---|
| smoke | 通信验证，passthrough/swap/gain2/gain3/gain5/speed2 |
| record | 录音保存，输出必填，不启动 DSP |
| file | WAV 处理；不勾选保存时播放，勾选则离线保存；支持界面全部算法 |
| audio | 实时采集处理播放，可选保存；支持除 speed2 外全部界面算法 |

空间算法仅使用 `spatial / dual`，基础算法保留
`passthrough / swap / gain2 / gain3 / gain5 / speed2`。界面不接受
`spatial40 / spatial60 / orbit / dual4060` 或数字别名，也不生成旧位置参数。

方向控件读取 `shared/hrir_catalog.h` 的 710 个方向：先选仰角，再选该仰角下
有效方位角；改变仰角后原方位不可用时必须重新选择，不插值、不自动取近邻。
单声源默认 0°／0°，双声源默认 A 0°／270°、B 0°／90°。

时长默认 5 秒，可选 1～2147 秒；勾选“持续运行”生成 `--seconds 0`。
例如文件单声源生成的 host 参数为：

```sh
app_host DSP file --input input.wav --output output.wav --algo spatial --elevation 0 --azimuth 0
```

输出文件名与算法重名时也无需旧版路径规避。所有参数仍以 `QStringList`
交给 QProcess，不交给 shell。

### 后续通信接入

`AudioBackend` 定义开始／停止、状态、日志、完成及波形事件；`DemoBackend`
实现无硬件演示，`BoardBackend` 包装现有 `PipelineRunner`。后续在板端适配器
增加 PCM 传输即可，绘图层不依赖进程、共享内存或 socket 协议。

`waveformReady(PcmChunk)` 的最小契约：`sessionId` 必须对应当前任务；
`stream` 为 0（输入）或 1（输出），`firstFrame` 为该流的绝对帧序号，
`sampleRate=44100`、`channels=2`，`samples` 为交错 PCM16。每次任务使用新
会话号；过期会话及倒序帧会被丢弃。跨线程连接使用 Qt queued connection，
不直接从通信线程访问控件。本轮未新增真实 PCM 传输或修改 host/DSP 协议。

非 record 的实际过程与 `run.sh` 一致，但由 Qt 分阶段执行：

```text
slaveloader startup DSP server_dsp.xe674
app_host DSP <mode> <args...>
slaveloader shutdown DSP
```

QProcess 使用参数列表，无 shell 解释；日志显示的命令仅用于阅读。
startup 失败不运行 host；host 退出后执行 shutdown；shutdown 失败显示失败。
涉及 DSP 的异常任务会禁用再次启动，需先核查 DSP/系统日志，必要时正常重启板子恢复
已验证环境，再重开界面，不把重开界面本身当作硬件恢复。

点击停止或运行中退出窗口会等待 host 的 `main: control ready`，向这个
子进程发送 SIGINT，等待其清理、退出，再关闭 DSP。初始化期间的停止
请求会保留。加载 DSP 时停止，则等加载结束再卸载，不启动 host。
30 秒无进展仅提示超时并保留界面，不强杀音频进程或 loader。
底层 ALSA/驱动调用可能阻塞，不能承诺停止有严格时间上限。

同一板端的 GUI 使用 `/tmp/ex04-qt-audio.lock` 文件锁阻止多个界面同时操作
音频；锁放在本地临时目录以适配 NFS 部署，不会删除锁文件，空闲时关闭
文件描述符即可释放锁。该锁不约束手动运行的其他程序，
GUI 运行音频时不要在其他终端并发执行 `run.sh` 或 host。

日志窗口最多保留 1500 行，可保存当前日志；需要长期完整 stdout/stderr
时保留终端重定向或板端诊断记录。

## IPC 验证

两个按钮分别验证信号量握手及共享内存数据交互，每次 10 条消息，约
200 ms 一条。C 进程写完后置“可读”，Qt 线程获取、复制数据后置“可写”，
避免覆盖尚未读取的消息；工作线程通过信号更新 GUI，20 ms 间隔非阻塞
轮询信号量，不阻塞界面线程。

每次会话由 Qt 用 `IPC_PRIVATE` 创建两个信号量及可选共享内存，权限 0600，
把 semid/shmid 作为参数交给同目录 `ex04_ipc_peer`。这些 ID 会写入界面
日志。Qt 是资源所有者，等待线程和子进程结束后删除资源；停止、启动
失败、辅助进程异常退出均走同一回收路径。示例超时 10 秒自动停止；
辅助进程停止后仍不退出，2 秒后仅强制结束这个示例子进程。

进程被外部 SIGKILL 或断电时无法保证执行用户态清理。先确定相关进程已
退出，再根据**该会话日志中的 ID**定向清理，不清除其他程序的资源：

```sh
ipcs -s
ipcs -m
ipcrm -s <本次会话的semid>
ipcrm -m <本次会话的shmid>
```

## Linux 测试

桌面调试依赖 Qt5 开发包，例如 Ubuntu20 的 `qtbase5-dev qt5-qmake
build-essential`。测试不需要显示器或目标板：

```sh
cd /path/to/ex04/frontend_arm
BUILD_TARGET=desktop sh build.sh /usr/bin/qmake
./build/bin/ex04_frontend_arm --windowed

mkdir -p build/tests
cd build/tests
/usr/bin/qmake ../../tests/tests.pro
make -j2
EX04_IPC_PEER="$(cd ../bin && pwd)/ex04_ipc_peer" ./test_frontend_arm
```

测试覆盖命令参数和路径特殊字符、模式限制、重复启动、跨窗口文件锁、
startup/host/shutdown 失败、停止期间的生命周期，以及 Qt/C IPC 的数据、
提前停止和异常退出。另有只依赖 GCC/Python3 的真实 System V IPC 测试：

```sh
cd /path/to/ex04
mkdir -p frontend_arm/build/bin
gcc -std=c99 -Wall -Wextra -Werror frontend_arm/ipc_peer/main.c \
    -o frontend_arm/build/bin/ex04_ipc_peer
python3 frontend_arm/tests/test_ipc_peer.py
python3 frontend_arm/tests/test_host_stop.py
```

最终上板还需检查：Qt4/Qt5 对应版本构建、ARM ABI/依赖、HDMI 显示和
鼠标、IPC 10 条消息、各业务模式、空间音效、停止后再次运行，以及现有
Qt/SysLink 共存测试。桌面或 C 示例通过不代表目标板共存测试通过。

本次本机验证结果见 [VALIDATION.md](VALIDATION.md)。

### 波形与 GUI 自动验收（桌面 Qt5）

```sh
cd /path/to/ex04/frontend_arm
PROJECT_ROOT=$(cd .. && pwd)
mkdir -p build/ui-tests
cd build/ui-tests
/usr/bin/qmake ../../tests/ui.pro
make -j2
QT_QPA_PLATFORM=offscreen EX04_PROJECT_ROOT="$PROJECT_ROOT" \
    EX04_SCREENSHOT_DIR="$PWD/screenshots" ./test_ui
```

需要可用的中文字体（优先文泉驿微米黑，其次 Noto Sans CJK SC／微软雅黑）。
测试会读取仓库 WAV，验证参数、方向、文件解析、峰值包络、游标、过期请求、
固定容量缓冲和演示生命周期，并对 800×480、1024×768 四页及运行场景截图。
截图来自实际 Qt Widgets 渲染，不代表板端 framebuffer／HDMI 已验收。
