# DM8168 板上 Qt 与 SysLink 共存可行性测试

目标：DM8168 的 HDMI 上持续运行 Qt 界面，同时 ARM 通过 SysLink 调用 DSP，返回数据正确，DSP 正常退出后显示仍然工作。

这是测试方案，不是已完成的板端测试报告。本文最初编写时未连接目标板；截至 2026-11-01，用户回传已确认 ARM Qt5.9.7、host/DSP 构建及探针上板启动，业务 GUI 的 VNC 中文和波形可显示。原始结果见 [构建与板端回传](evidence/arm-build-2026-11-01/README.md)。本文的 HDMI、鼠标及显示/DSP 共存验收仍未完成。

## 1. 当前工程中的关键事实

- 当前板端 GUI 是 `frontend_arm/`；已停用的 SSH/Telnet 桌面前端已清理。
- 共存测试期间保留显示固件，按下文分阶段检查 DSP 生命周期。
- 根目录 `run.sh` 没有停显示固件，只调用 DSP startup、host 应用和 DSP shutdown。
- 本工程音频通信使用 **Notify + SharedRegion**，不是 MessageQ，也不是 Linux System V 共享内存。
- 当前 DSP 固件一次加载只服务一个 host 会话。每次 host 结束后必须卸载、重新加载 DSP，不能只加载一次再连续启动多个 host。
- `run.sh` 在 shutdown 失败时仅打印 warning，仍返回 host 状态。因此不能只根据它返回 0 就判定整个生命周期通过。

测试顺序：核对内存及固件 → Qt/DSP 各自基线 → 分阶段共存 → 重复启动 → 实际音频负载。任一阶段失败，先定位该阶段，不继续叠加后续功能。

## 2. 准备环境和保存现场

需要：DM8168、HDMI 显示器、USB 鼠标（键盘可选）、串口终端、开发 Linux 虚拟机，以及与板上运行库匹配的 ARM Qt qmake。准备两个板端命令会话；串口保留为独立观察通道。

以下 `[板端]` 在 DM8168 Linux 执行，`[虚拟机]` 在开发 Linux 执行。先保留能正常启动的系统、启动参数及应用副本；实验改动使用副本，初期不持久化 U-Boot 环境、不刷写固件。

**[板端]** 正常重启后执行。`APPDIR` 是项目现有部署约定，实际不同就先修改。`/tmp` 日志在重启前需要复制到虚拟机保存。

```sh
APPDIR=/home/root/ex04_syslink_dsp/release
LOGDIR=/tmp/qt-syslink-$(date +%Y%m%d-%H%M%S)-$$
mkdir -p "$LOGDIR"
cd "$APPDIR" || exit 1
printf '%s\n' "$LOGDIR"

uname -a > "$LOGDIR/uname.txt"
cat /proc/cmdline > "$LOGDIR/cmdline.txt"
cat /proc/iomem > "$LOGDIR/iomem.txt"
cat /proc/meminfo > "$LOGDIR/meminfo.txt"
cat /proc/modules > "$LOGDIR/modules.txt"
cat /proc/fb > "$LOGDIR/fb.txt"
ps > "$LOGDIR/processes.txt"
dmesg > "$LOGDIR/dmesg-boot.txt"
cat /etc/init.d/load-hd-firmware.sh > "$LOGDIR/load-hd-firmware.sh.txt"
cat ./run.sh > "$LOGDIR/run.sh.txt"
ls -l /dev/fb* > "$LOGDIR/fb-devices.txt" 2>&1
md5sum ./app_host ./server_dsp.xe674 ./slaveloader > "$LOGDIR/binaries.md5"
```

`md5sum` 仅记录二进制版本，板上没有时在虚拟机记录。保存串口完整日志，不只截取最后一个错误。

阅读 `load-hd-firmware.sh` 及其调用的脚本，记录启动时加载了哪些 DSP/M3 镜像、参数及管理进程。`ps` 看不到固件，不等于 DSP/M3 没运行。

## 3. 加载 DSP 前核对实际内存和固件管理

本工程 `shared/config.bld` 使用下列地址；结束地址不包含在区间内：

| 区域 | 起始 | 结束（不包含） | 大小 |
|---|---|---|---|
| SR0 | `0x8E000000` | `0x8E010000` | 64 KiB |
| SR1 | `0x8E010000` | `0x8F000000` | 约 15.94 MiB |
| DSP_PROG | `0x8F000000` | `0x90000000` | 16 MiB |

`Dsp.cfg` 有一处 SR0 注释写成 192 KB，实际以数值定义及生成配置为准。源码中 Linux 128 MB 的注释也不能代替实际运行配置。

**[虚拟机]** 在已构建的工程目录检查最终配置：

```sh
rg -n 'base:|len:|SR_0|SR_1|DSP_PROG' shared/config.bld
rg -n 'SharedRegion|ownerProcId|cacheEnable|sr0MemorySetup|procSync|MAR128' dsp/Dsp.cfg
rg -n 'MEMORY|SR_0|SR_1|DSP_PROG|8e|8E|8f|8F' \
  dsp/configuro/linker.cmd dsp/bin/release/obj/server_dsp.xe674.map
```

尚未构建就先用工程原有工具链构建；配置改动后也必须重编译，不能测试旧镜像。

结合板端启动日志、内核保留区、SDK 内存配置、固件链接图和 loader 参数，补全：

| 使用者 | 实际物理区域 | 依据 | 与本工程区域是否重叠 |
|---|---|---|---|
| Linux 可分配内存 | 待查 | cmdline、iomem、内核保留配置 | 待查 |
| VPSS-M3 程序、数据和 IPC | 待查 | 匹配版本的固件/loader 配置 | 待查 |
| 其他已启动核/固件 | 待查 | 启动脚本、固件配置 | 待查 |
| Framebuffer/VRAM、CMEM（若使用） | 待查 | 驱动参数、日志、SDK 配置 | 待查 |
| 本工程 DSP 程序和共享区 | 上表 | 最终 map、运行时 SharedRegion 配置 | 待查 |

必须明确：

1. `/proc/iomem` 不一定列出所有远端固件占用，未列出不代表空闲。
2. 假如 Linux 实际管理从 `0x80000000` 开始的连续 256 MiB，且没有另外保留，本工程三段就会落入 Linux 内存，必须先消除重叠。
3. 整个系统对 SharedRegion 编号、地址、长度、owner 和缓存属性的理解必须兼容。不能重新定义其他核正在使用的共享区。
4. 确认 DSP 是否已被开机固件或其他业务占用。若已占用，先制定只针对 DSP 的停用/接管流程，保留 VPSS 服务；不能直接覆盖正在运行的 DSP。
5. 不照抄网上的 `mem=169M`、`mem=128M` 或任意新物理地址。降低 Linux 内存也不会自动消除与其他固件的冲突。

**通过条件：** 地址、共享区和固件管理关系均已核对兼容。信息不足或发现重叠，先停在此步整理配置；这不等于已经证明 Qt 与 SysLink 不可共存。

## 4. Qt 单独基线：编译并运行刷新探针

源码在 `tests/qt_syslink_probe/`。探针每 100 ms 更新计数、色块交替变色、鼠标点击增加计数，约每秒向 stderr 输出 `QT_HEARTBEAT`。Escape 正常关闭。它不调用 DSP、ALSA、网络或业务 IPC。

**[虚拟机]** 把占位路径改成真实 ARM Qt 工具链路径，不能使用桌面 Qt 的 qmake：

```sh
cd /path/to/ex04/tests/qt_syslink_probe
QMAKE_ARM=/actual/path/to/arm-qt/bin/qmake
"$QMAKE_ARM" -query
"$QMAKE_ARM" qt_syslink_probe.pro
make -n
make
file qt_syslink_probe
```

`make -n` 应显示 ARM 编译器；`file` 应确认 ARM ELF。Qt4/Qt5 分别与板端对应运行库配套。通过现有 NFS 或其他部署方法把程序放到板端 `$APPDIR/qt_syslink_probe`。

**[板端]** 只退出占用显示的桌面/Matrix 应用，保留显示固件及驱动。默认 EZSDK 可执行：

```sh
/etc/init.d/matrix-gui-e stop
```

脚本不存在则按实际系统退出对应 GUI，不用 `load-hd-firmware.sh stop` 替代。以下二选一：

```sh
# Qt4/Embedded
./qt_syslink_probe -qws > "$LOGDIR/qt.log" 2>&1 &
QT_PID=$!
```

```sh
# Qt5：板上需已有可用的 linuxfb 插件及依赖
./qt_syslink_probe -platform linuxfb > "$LOGDIR/qt.log" 2>&1 &
QT_PID=$!
```

沿用该板已验证的 framebuffer、鼠标及运行库环境；初次验证不同时引入 EGLFS/SGX 或更换 Qt 主版本。

**通过条件：** 至少观察 2 分钟，HDMI 色块和时间不断变化，点击数增加，`tail "$LOGDIR/qt.log"` 可见连续心跳，无黑屏花屏。日志继续但画面静止不算通过；心跳频率也不是硬实时保证。

若未通过，先解决 Qt 版本、二进制架构、运行库、平台插件、字体、输入设备或 framebuffer，此时还不能归因于 SysLink。

## 5. DSP 单独基线

相同二进制已有成功日志可直接作为基线。否则，在已核对内存的已知可用 DSP 环境运行：

```sh
./run.sh smoke passthrough > "$LOGDIR/dsp-baseline.log" 2>&1
DSP_BASE_RC=$?
printf 'DSP_BASE_RC=%s\n' "$DSP_BASE_RC"
cat "$LOGDIR/dsp-baseline.log"
```

应有 5 轮正确数据校验、`main: status=0`，且无 shutdown warning。若基线只能在关闭显示固件后通过，保存日志，再正常重启到显示环境、重新设置 `LOGDIR`、启动探针，之后进行共存测试。内存核对未通过时不能强行运行此步骤。

## 6. 一次共存测试：分开检查四个阶段

逐条执行。每一步检查返回码、Qt 动态画面、点击及新增内核错误。失败就停，不把整段命令不加判断地一次粘贴执行。

### 6.1 复用现有 SysLink 模块

```sh
cat /proc/modules
```

已经加载则复用，不卸载重载。只有确认未加载、不是内建等情形时，才加载与现有系统配套的模块：

```sh
modprobe syslink > "$LOGDIR/modprobe.log" 2>&1
MOD_RC=$?
printf 'MOD_RC=%s\n' "$MOD_RC"
```

通过：模块可用，Qt 继续刷新，显示驱动未被卸载。不要混用不同 SDK 版本的模块、库和固件。

### 6.2 加载并启动 DSP

确认 DSP 可以由本测试接管后：

```sh
./slaveloader startup DSP server_dsp.xe674 > "$LOGDIR/startup.log" 2>&1
START_RC=$?
printf 'START_RC=%s\n' "$START_RC"
cat "$LOGDIR/startup.log"
dmesg > "$LOGDIR/dmesg-after-startup.txt"
```

观察约 10 秒。host 尚未连接时，DSP 等待 IPC 连接是预期现象。

通过：返回 0、画面持续刷新、鼠标有效、系统仍响应。此时就黑屏，优先查地址覆盖、处理器 ID、loader 的影响范围及已有固件，不先修改音效算法。

### 6.3 建立 IPC 并校验数据

```sh
./app_host DSP smoke passthrough > "$LOGDIR/smoke.log" 2>&1
HOST_RC=$?
printf 'HOST_RC=%s\n' "$HOST_RC"
cat "$LOGDIR/smoke.log"
dmesg > "$LOGDIR/dmesg-after-host.txt"
```

预期：`start callback ok`、`syslinkDspInit: ready`、round 0 到 4、正确校验及 `main: status=0`。

通过：全部数据校验正确、返回 0、Qt 显示和输入正常，host 退出后 Qt 也正常。

注意：虽然事件等待有超时，本工程 Notify 注册等待循环和 loader/内核调用仍可能阻塞，不能保证所有卡住都在 5 秒内退出。明显无进展时从第二会话保存日志及 dmesg，不并发启动第二个 host，也不重复加载 DSP。

### 6.4 卸载 DSP

```sh
./slaveloader shutdown DSP > "$LOGDIR/shutdown.log" 2>&1
STOP_RC=$?
printf 'STOP_RC=%s\n' "$STOP_RC"
cat "$LOGDIR/shutdown.log"
dmesg > "$LOGDIR/dmesg-after-shutdown.txt"
```

继续观察 Qt 30 秒并点击。

通过：返回 0，显示和输入继续工作，无新增 oops、固件异常或驱动错误。只有退出时失败，检查 STOPCALLBACK、SysLink_destroy、loader 生命周期和资源所有权；不要未经定位就删除清理调用。

## 7. 重复性与启动顺序测试

先手动完成 3 次 `startup → app_host DSP smoke swap → shutdown`，每条命令检查返回码后再继续。`swap` 会校验实际变换的数据，补充透传测试。

三次通过后定义函数，保持 Qt 运行，执行 100 次完整周期：

```sh
run_checked_cycle()
{
    case_tag=$1
    shift
    ./slaveloader startup DSP server_dsp.xe674 > "$LOGDIR/$case_tag-start.log" 2>&1
    start_rc=$?
    printf '%s start=%s\n' "$case_tag" "$start_rc" >> "$LOGDIR/cycles.txt"
    [ "$start_rc" -eq 0 ] || return 1

    ./app_host DSP "$@" > "$LOGDIR/$case_tag-host.log" 2>&1
    host_rc=$?
    printf '%s host=%s\n' "$case_tag" "$host_rc" >> "$LOGDIR/cycles.txt"
    # 错误时先停下来取证，不在未知 IPC 状态下自动继续。
    [ "$host_rc" -eq 0 ] || return 1

    ./slaveloader shutdown DSP > "$LOGDIR/$case_tag-stop.log" 2>&1
    stop_rc=$?
    printf '%s stop=%s\n' "$case_tag" "$stop_rc" >> "$LOGDIR/cycles.txt"
    [ "$stop_rc" -eq 0 ] || return 1
}

n=1
while [ "$n" -le 100 ]; do
    if ! run_checked_cycle "cycle-$n" smoke swap; then
        printf 'FAILED: cycle %s; stop and collect logs\n' "$n"
        break
    fi
    n=$((n + 1))
    sleep 1
done
dmesg > "$LOGDIR/dmesg-after-cycles.txt"
cat /proc/meminfo > "$LOGDIR/meminfo-after-cycles.txt"
```

通过：100 次 start/host/stop 均为 0、Qt 持续变化、无持续内存损失或新增错误。MemFree 的缓存波动不等于泄漏。

再试相反启动顺序：正常关闭 Qt → startup DSP → 启动 Qt → 一次 host smoke → shutdown DSP。每个 DSP 镜像仍只服务一个 host 会话。

最后做 3 次正常系统重启，每次重新记录环境并验证一个完整共存周期，避免依赖残留状态。

## 8. 加入音频负载

沿用上面的函数，Qt 保持运行。先确认 ALSA 声卡与输入路由正常，mixer 设置单独执行，不使用 Fix Issues。以下每条成功后再执行下一条：

```sh
run_checked_cycle audio-pass audio 10 passthrough
run_checked_cycle audio-space audio 10 spatial40
run_checked_cycle audio-long audio 600 spatial40
```

空间模式需要实际协商的配置为 44100 Hz、双声道、16 bit。也可先用符合格式的 WAV 排除声卡因素：

```sh
run_checked_cycle file-space file input.wav "$LOGDIR/spatial-output.wav" spatial40
```

指定输出文件的 `file` 模式是离线渲染，不播放，不能证明实时性。

通过标准：

- start/host/stop 全部返回 0；声音正常，无持续断音。
- host 结束输出 `audio status=0`、`blocks>0`、`dropped=0`。
- 完整日志无 ALSA underrun/overrun/recovery 异常，不能只看 dropped。
- Qt 刷新、点击正常，没有持续超过约 1 秒的可见冻结（建议的初步验收指标）。
- 无新增内核和固件异常。

只在负载下失败时，以同一镜像、算法、声卡参数对比 Qt 开/关，显示固件与内存配置不变，区分原有音频性能问题和 GUI 的增量负载。

测量实时性时使用单调时钟分别记录 DSP 往返、排队及整条流水线耗时。块预算是 `实际 periodFrames / 实际 sampleRate`：1024/44100 约 23.22 ms，256/44100 约 5.80 ms。应关注峰值、队列积压与丢块，不能只看平均值。10 分钟通过只代表阶段性可行。

## 9. 故障定位与配置修改范围

| 现象 | 优先检查 |
|---|---|
| Qt 单独失败 | 二进制架构、Qt 库/插件、显示与输入 |
| DSP 单独失败 | 原有固件、工具链与 IPC 配置 |
| DSP startup 立即黑屏 | 内存覆盖、DSP 已有所有者、loader 影响范围 |
| load/start callback 失败 | SharedRegion、处理器列表、IPC/驱动版本 |
| Qt 心跳继续但 HDMI 冻结 | VPSS、显示驱动、显示缓冲区 |
| 显示正常但返回数据错误 | SharedRegion、缓存一致性、Notify 协议 |
| host 退出后黑屏 | STOPCALLBACK、清理时序和资源引用 |
| shutdown 后黑屏 | loader 停止对象、全局资源生命周期 |
| 首次成功、再次失败 | attach/detach、共享堆和资源释放 |
| smoke 成功、音频失败 | ALSA、采样格式、算法预算与 ARM 负载 |

若需迁移内存：依据实际 DDR 和各固件占用选择已确认区域，同步检查 `shared/config.bld`、最终 DSP map、`dsp/Dsp.cfg` 的 SharedRegion/MAR 缓存配置，以及 host/loader/驱动映射。移动 SR0 可能影响多个核，不能只移动 DSP 镜像。

`Ipc.procSync` 当前已经是 PAIR，不要为了共存随意改为 ALL。MultiProc 编号必须与整个系统一致。改配置后重新构建部署，从内存核对和各自基线重新测试。

失败后先保存日志。状态不明时不要反复加载或把 kill -9 当正常清理；保存 `/tmp` 证据后通过已知正常启动方式重启，恢复可复现环境。

## 10. 记录和结论

| 测试项 | 环境/镜像版本 | 通过、失败或未执行 | 日志及现象 |
|---|---|---|---|
| 内存和固件核对 | | | |
| Qt 单独 2 分钟 | | | |
| DSP 单独 smoke | | | |
| 共存 startup/host/shutdown | | | |
| 100 次完整周期 | | | |
| 相反启动顺序 | | | |
| 3 次重启复验 | | | |
| 10 分钟实际音频 | | | |

- 共存 smoke 通过：证明该配置下显示与 DSP 通信可以共存，完成原理验证。
- 重复生命周期和音频也通过：支持继续采用板上 Qt + 当前 SysLink。
- 仅 Qt 与 Linux 进程通信例程通过：不能证明 DSP/显示共存。
- 配置不兼容或某阶段失败：记录第一处失败证据，不直接认定 Qt 与 SysLink 天生冲突。

## 依据和未确定项

TI 的 EZSDK 5.05 指南说明：默认 SysLink 示例内存图与 EZSDK 不同，运行前要求停止图形和固件。这是本测试优先核对系统配置，而非直接删除 stop 命令的依据。

来源：[DM816x EZ Software Developers Guide，5.05.01.04，第 6 页](https://software-dl.ti.com/dsps/dsps_public_sw/ezsdk/5_05_01_04/exports/DM816x_EZ_Software_Developers_Guide.pdf)。

实际 DDR 容量、VPSS 固件映射、SysLink 内核配置和 ARM Qt 工具链尚未取得，因此本方案不提供未经验证的新物理地址或一键修改启动参数的命令。
