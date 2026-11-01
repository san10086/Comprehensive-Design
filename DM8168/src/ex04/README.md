# EX04 空间音效处理工程

文档入口见 [文档索引](docs/README.md)，本次整理内容见 [工作目录整理报告](docs/workspace-cleanup-2026-11-01.md)。

## Qt 前端状态

- `frontend_arm/`：当前使用的 ARM 板端 Qt 工程，在 Ubuntu20 中交叉编译，在 DM8168 上运行。构建、部署及 IPC 验证见 [ARM Qt 说明](frontend_arm/README.md)。
- 原桌面前端已清理；历史实现可从 Git 历史或本次清理备份查看。

环境搭建请按 [ARM Qt 环境配置与操作报告](docs/arm-qt-environment-setup-report.md) 操作，包含 Windows、Ubuntu20 虚拟机、DM8168 板端的分步命令与验收标准。

从当前源码开始更新与部署，见 [空间处理与 Qt 更新、编译、打包说明书（XFTP）](docs/空间处理与Qt更新编译打包说明书_XFTP.md)：Windows 源码打包、XFTP 上传、host/DSP 与 Qt5.9.7 构建、发布包、板端 VNC 及后续增量更新。

ALSA 模块来源、适配增强与旧命令对应关系已整合至 [ALSA 模块承接记录](docs/alsa-integration-history.md)。

# 当前模块结构

```text
ex04/
  frontend_arm/             当前 ARM 板端 Qt GUI、Linux 进程控制及 IPC 验证
  host/
    app/main.c              ARM 侧主程序，解析 smoke/record/file/audio 和算法 mode
    syslink/syslinkDsp.*    ARM 侧 SysLink 封装，发送音频配置和 APP_ALGO_MODE_*
    audio/*                 ALSA/WAV 音频模块
    makefile                ARM app_host 构建脚本
  dsp/
    Server.c                DSP 侧 Notify/SharedRegion 服务，接收算法 mode 并调用 AudioAlgo
    AudioAlgo.c/.h          DSP 算法统一入口/适配层，保持旧 frame/config/stats 设计
    dsp_algorithm.c/.h      算法同学提供的 S16_LE stereo helper：passthrough/swap/gain2/gain3/gain5/speed2
    dsp_spatial.c/.h        2048 点 FFT overlap-add 双耳卷积、跨块尾部和 710 组内嵌 HRIR
    main_dsp.c              DSP 入口
    makefile                DSP server_dsp.xe674 构建脚本，已编入基础与空间算法
  shared/
    AppCommon.h             ARM/DSP 共用命令、payload 定义和 APP_ALGO_MODE_* 常量
    SystemCfg.h             Notify line/event 配置
    config.bld              DSP 平台内存配置
  tests/spatial/            纯 C/Q15 空间卷积参考工程、自测和 MATLAB 对照脚本
  run.sh                    ARM rootfs 上运行脚本，负责启动/关闭 DSP image
  makefile                  顶层构建/安装脚本
  products.mak              TI/SysLink 工具链路径配置
```

空间音效算法在 `tests/spatial/` 离线验证；DSP 侧实现位于
`dsp/dsp_spatial.c`，由 `AudioAlgo_process()` 调用。测试文档给出了 HRIR
顺序、分块连续性、MATLAB 定点对照、DSP 原地处理回归及板端试听步骤。

# 主程序运行模式

## HRIR 方位库与具名参数

完整数据源位于 `assets/hrir/mit_kemar`：1420 个 HRIR WAV 配成 710 个方向，
覆盖仰角 -40° 到 90°。11 个耳机/扬声器响应 WAV 不属于方向数据，不参与编译。
系数按原始 PCM16 数值编入 DSP，合计 1,454,080 字节（约 1.39 MiB）；
ARM 只编入方向目录。DSP 仅缓存当前一组或两组 HRIR 的 FFT 频谱。

```sh
# 查看编入的方向，无须加载 DSP
./run.sh directions

# 单声源：两个输入声道平均后定位到指定方向
./run.sh file --input input.wav --output out.wav \
  --algo spatial --elevation 60 --azimuth 250
./run.sh audio --seconds 10 --algo spatial --elevation 0 --azimuth 90

# 双声源：输入左声道为 A，右声道为 B，各以 0.5 权重混合
./run.sh file --input sources.wav --output out.wav --algo dual \
  --a-elevation 60 --a-azimuth 250 --b-elevation 40 --b-azimuth 45

# 其他算法及录音同样支持具名参数
./run.sh file --input input.wav --output out.wav --algo swap
./run.sh record --output record.wav --seconds 5
./run.sh smoke --algo passthrough

# 仅校验命令，不加载 DSP、不打开音频设备
./run.sh audio --seconds 10 --algo spatial --elevation -40 --azimuth 45 --check
```

仰角、方位角必须为整数，范围分别为 -90..90、0..359，且必须出现在
`directions` 列表中；不存在时直接报错，不自动选择近邻、不插值。
沿用 MIT 坐标约定：仰角 0° 为水平、90° 为头顶；方位角 0° 前方、90° 右侧、
180° 后方、270° 左侧。`spatial` 必须提供一组角度，`dual` 必须提供 A/B 两组角度。
文件输入仍需为 44.1 kHz、双声道 PCM16；时长默认 5 秒，0 表示持续运行，最大 2147 秒。

旧位置参数、算法及数字别名继续可用；`spatial40`、`spatial60`、`dual4060`
和旧语法中的 `dual` 保持原固定方向，`orbit` 保持原四秒镜像往返。
新具名语法中的 `--algo dual` 使用用户指定的 A/B 方向。一次调用不能混用新旧语法。
Qt 界面已改为仅生成新版具名参数，支持按方向库选择单声源与双声源角度；
旧固定方向入口不再出现在界面中。新版界面默认演示环境，提供真实 WAV
四轨对比及动态波形；板端模式复用现有进程控制，实时 PCM 通信保留接口。
界面操作、构建与截图验收见 [ARM Qt 说明](frontend_arm/README.md)。

### 更新 HRIR 数据

将新的配对文件放入 `assets/hrir/mit_kemar` 的子目录，采用
`L60e250a.wav` / `R60e250a.wav` 这类命名，负仰角如 `L-40e045a.wav`。
生成器要求单声道、44.1 kHz、PCM16、512 点，保留自然采样顺序和幅值。
缺耳、重复方向或格式错误会导致生成失败，保留已有生成文件。
现有兼容命令需要的 `40e045`、`60e250` 必须保留。

```sh
make hrir
make hrir-check
# 可覆盖源目录；路径含空格时需要引号
make hrir HRIR_SOURCE="assets/hrir/mit_kemar"

make -C tests/spatial test test-catalog
python3 tests/spatial/test_spatial_protocol.py
python3 frontend_arm/tests/test_host_stop.py

# 在已配置 TI/ARM 工具链的构建环境中
make
make install
```

生成文件已纳入源码，普通交叉编译无需运行 Python；再次生成和完整目录测试需要原始 WAV。
**ARM 程序与 DSP 固件必须成套重新编译、部署**，不能只替换 WAV 或只部署一端。
方位编码为 `360 * (elevation + 90) + azimuth`，不会因添加数据而改变。
启动时经 SysLink 发送方位并确认后才选择算法；方向在一次运行内固定。
新增配置失败会明确返回错误，旧固件不支持新命令时按现有通信超时退出。
部署后先用 `directions` 和 `--check` 检查，再验证文件输出和耳机实时试听；
完整 DSP 链接是否满足 16 MiB 程序区限制，以目标工具链的 map 文件为准。

`host/app/main.c` 支持四种模式。除 `record` 外，都会经过 SysLink 启动
`server_dsp.xe674` 并调用 DSP。`file` 支持
`passthrough`、`swap`、`gain2`、`gain3`、`gain5`、`speed2`、
`spatial40`、`spatial60`、`orbit`、`dual4060`；`audio` 除 `speed2` 外也支持上述模式，
`smoke` 只支持非空间模式。
不传时默认 `passthrough`。

### smoke

用途：验证 ARM 和 DSP 的基本 SysLink 通信。

命令：

```sh
./run.sh smoke [passthrough|swap|gain2|gain3|gain5|speed2]
```

The optional mode selects which DSP algorithm smoke should configure and verify.

执行链路：

```text
ARM 构造测试 PCM -> DSP selected algorithm -> ARM 校验返回值
```

典型输出：

```text
main: round 0, frameCount=8
main: before DSP: ...
main: after DSP : ...
main: DSP <mode> verified
<-- main: status=0
```

### record

用途：只录音保存 WAV，不启动 DSP。

命令：

```sh
./run.sh record 1.wav 3
```

执行链路：

```text
ALSA Capture -> WAV 文件
```

参数：

| 参数 | 含义 |
|---|---|
| `test.wav` | 输出 WAV 文件 |
| `5` | 录音秒数；不填时默认 5 秒 |

典型输出：

```text
main: requested record cfg ...
main: actual record cfg ...
[Capture] start recording 5000 ms ...
<-- main: record status=0, blocks=...
```

### file

用途：ARM 选择一个 WAV 文件并发送给 DSP 处理。未指定输出文件时实时
播放；指定输出文件时离线保存，完成后再用 `aplay` 播放，避免较慢算法
造成 ALSA underrun。

命令：

```sh
./run.sh file test.wav [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]
./run.sh file test.wav out.wav [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]
```

Mode argument is optional. Omit it for `passthrough`; use `swap` for stereo
L/R swap, `gain2`/`gain3`/`gain5` for saturated S16_LE gain, or `speed2`
for fixed-size 2x-speed file/smoke demo. `spatial40` and `spatial60` select the
two embedded 512-tap binaural HRIR pairs. `orbit` moves the `40e045` response
to its left/right-ear-swapped mirror and back over a smooth four-second cycle.
`dual4060` treats input left as independent source A at `60e250` and input
right as independent source B at `40e045`, then mixes both binaural renders
with 0.5 gain per source.

执行链路：

```text
无 out.wav：WAV -> ARM -> DSP -> ARM -> ALSA Playback
有 out.wav：WAV -> ARM -> DSP -> ARM -> out.wav
```

参数：

| 参数 | 含义 |
|---|---|
| `test.wav` | 输入 WAV 文件 |
| `out.wav` | 可选，保存 DSP 返回后的 WAV |

注意：

* `swap`, `gain2`, `gain3`, `gain5`, and `speed2` currently require S16_LE stereo PCM. The DSP-side
  `AudioAlgo_process()` rejects non-`channels == 2` or non-`bitDepth == 16`
  payloads for these modes, so ARM may accept the command line but processing
  can fail at runtime if ALSA/WAV negotiation uses another format.
* In `file` mode, the third positional argument is ambiguous with mode names.
  `./run.sh file input.wav swap` is parsed as mode `swap`, not as an output file
  named `swap`. To write to such a filename, pass an explicit mode too, for
  example `./run.sh file input.wav swap passthrough`.

- 输入 WAV 必须和当前平台音频配置匹配。
- 当前 DM8168 默认配置为 44100 Hz、2 声道、16 bit、1024 帧 period。
- WAV 读取会检查 `blockAlign / byteRate / dataSize`。
- 离线生成后使用 `aplay -D default out.wav` 检查效果。

典型输出：

```text
main: wav cfg rate=44100 ch=2 bit=16 period=1024 payload=4096
syslinkDspInit: audio cfg rate=44100 ch=2 bit=16
main: file block: ...
<-- main: file status=0, blocks=..., frames=220500/220500
```

### audio

用途：实时录音，送 DSP，返回后播放。

命令：

```sh
./run.sh audio 5 [passthrough|swap|gain2|gain3|gain5|spatial40|spatial60|orbit|dual4060]
```

Mode argument is optional. `audio` also supports both spatial modes;
`speed2` is intentionally limited to `smoke` and `file`.

执行链路：

```text
ALSA Capture -> ARM -> DSP -> ARM -> ALSA Playback
```

参数：

| 参数 | 含义 |
|---|---|
| `5` | 运行秒数；为 0 或不传时可进入无限录音模式 |

当前行为：

1. 初始化 capture，拿到 ALSA 实际采集参数。
2. 使用采集实际参数初始化 playback。
3. 检查 capture/playback 的 `sampleRate / channels / bitDepth` 是否一致。
4. 初始化 DSP。
5. 开始阻塞式采集，每个 period 送 DSP，返回后播放。

典型输出：

```text
main: actual capture cfg rate=44100 ch=2 bit=16 period=1024 payload=4096
main: actual playback cfg rate=44100 ch=2 bit=16 period=1024 payload=4096
main: starting blocking ALSA -> DSP -> ALSA pipeline
main: audio block: ...
<-- main: audio status=0, blocks=...
```

# 程序应用接口

### `run.sh`

`run.sh` 是 ARM rootfs 上推荐使用的运行入口。

```sh
./run.sh smoke
./run.sh record output.wav [seconds]
./run.sh file input.wav [output.wav] [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]
./run.sh audio [seconds] [passthrough|swap|gain2|gain3|gain5|spatial40|spatial60|orbit|dual4060]
```

行为：

- `record` 模式不启动 DSP。
- 其他模式会执行：

```text
slaveloader startup DSP server_dsp.xe674
app_host DSP ...
slaveloader shutdown DSP
```

`run.sh` 会检查 DSP startup 是否成功；shutdown 失败时打印 warning，但保留 `app_host` 的退出码。

### `app_host`

也可以直接调用 `app_host`：

```sh
./app_host DSP smoke
./app_host DSP audio 5
./app_host record output.wav 5
./app_host DSP file input.wav [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]
./app_host DSP file input.wav output.wav [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]
```

一般不建议手动直接调 `app_host DSP ...`，因为还需要自己先启动 DSP image。使用 `run.sh` 更稳。

### Host SysLink C API

文件：`host/syslink/syslinkDsp.h`

```c
int syslinkDspInit(const syslinkDspConfig *cfg);
int syslinkDspProcessBuffer(const void *inData, void *outData, int byteCount);
int syslinkDspProcessFrames(const void *inData, void *outData, int frameCount);
int syslinkDspProcess(const short *inPcm, short *outPcm, int frameCount);
int syslinkDspClose(void);
```

推荐接口：

```c
syslinkDspProcessFrames(inData, outData, frameCount);
```

说明：

- `syslinkDspProcessBuffer()` 以字节数为单位，适合非 `short *` PCM 或调用者已经知道 payload 字节数的情况。
- `syslinkDspProcessFrames()` 以音频帧数为单位，会使用 `cfg.frameBytes` 计算 payload。
- `syslinkDspProcess()` 是旧 S16_LE 兼容包装。

`syslinkDspConfig`：

```c
typedef struct {
    const char *remoteProcName;
    int frameBytes;
    int bufferBytes;
    int sampleRate;
    int channels;
    int bitDepth;
    int algorithmMode;
} syslinkDspConfig;
```

字段说明：

| 字段 | 含义 |
|---|---|
| `remoteProcName` | 通常为 `"DSP"` |
| `frameBytes` | 单帧字节数，`channels * bitDepth / 8` |
| `bufferBytes` | 单次最大处理 payload 字节数 |
| `sampleRate` | 采样率 |
| `channels` | 声道数 |
| `bitDepth` | 每 sample 位宽 |

`algorithmMode` uses the shared constants from `shared/AppCommon.h`:

| Value | Meaning |
|---|---|
| `APP_ALGO_MODE_PASSTHROUGH` | keep PCM unchanged |
| `APP_ALGO_MODE_SWAP_STEREO` | swap interleaved stereo L/R samples |
| `APP_ALGO_MODE_GAIN_X2` | saturated 2x S16_LE gain |
| `APP_ALGO_MODE_SPEED2` | fixed-size 2x-speed file/smoke demo |
| `APP_ALGO_MODE_GAIN_X3` | saturated 3x S16_LE gain |
| `APP_ALGO_MODE_GAIN_X5` | saturated 5x S16_LE gain |
| `APP_ALGO_MODE_SPATIAL_40E045` | 512-tap binaural FIR using the 40e045 pair |
| `APP_ALGO_MODE_SPATIAL_60E250` | 512-tap binaural FIR using the 60e250 pair |
| `APP_ALGO_MODE_ORBIT` | four-second smooth motion between 40e045 and its ear-swapped mirror |
| `APP_ALGO_MODE_DUAL_60_40` | input L at 60e250 plus independent input R at 40e045 |

### ALSA/WAV C API

文件：`host/audio/`

录音：

```c
int audioCapInit(audioConfig *cfg);
int audioCapSetCallback(audioCapCallback cb, void *userData);
int audioCapStart(int durationMs);
int audioCapStop(void);
void audioCapClose(void);
```

播放：

```c
int audioPbInit(audioConfig *cfg);
int audioPbWrite(short *pcmData, int frameCount);
int audioPbDrain(void);
void audioPbClose(void);
```

WAV：

```c
int audioWavOpenWrite(audioWavFile *wav, const char *fileName,
                      int sampleRate, int channels, int bitDepth);
int audioWavOpenRead(audioWavFile *wav, const char *fileName);
int audioWavWriteFrames(audioWavFile *wav, short *pcmData, int frameCount);
int audioWavReadFrames(audioWavFile *wav, short *pcmBuf, int maxFrames);
void audioWavClose(audioWavFile *wav);
```

注意：

- `audioCapInit()` 和 `audioPbInit()` 会回写 ALSA 实际参数到 `audioConfig`。
- `audioWavReadFrames()` / `audioWavWriteFrames()` 当前仍以 `short *` 为参数，主链路默认 S16_LE。

# 给 DSP 算法同学的预留接口

DSP 算法入口在：

```text
dsp/AudioAlgo.h
dsp/AudioAlgo.c
```

当前 DSP Server 会在每次收到 ARM payload 后构造 `AudioAlgoFrame`，然后调用：

```c
AudioAlgo_process(&Module.audioAlgo, &frame);
```

### 算法配置

```c
typedef struct {
    UInt32 sampleRate;
    UInt16 channels;
    UInt16 bitDepth;
    UInt16 frameBytes;
    UInt32 maxFrameCount;
    UInt32 reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoConfig;
```

用途：

- DSP 端收到 ARM 音频配置后，会调用 `AudioAlgo_configure()`。
- 当前 ARM 会发送 `sampleRate / channels / bitDepth`。
- `frameBytes = channels * bitDepth / 8`。
- `maxFrameCount` 用于限制单次处理最大帧数。

### 算法参数

```c
typedef struct {
    UInt32 mode;
    UInt32 flags;
    Int32  gainQ15;
    Int32  reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoParams;
```

用途：

- 预留算法模式、开关、增益等运行参数。
- 当前默认：

```c
mode = AUDIO_ALGO_MODE_PASSTHROUGH;
flags = AUDIO_ALGO_FLAG_BYPASS;
gainQ15 = AUDIO_ALGO_GAIN_UNITY_Q15;
```

后续如果要支持降噪、滤波、增益、模式切换，可以扩展该结构。

### 单帧处理上下文

```c
typedef struct {
    const Void  *inputData;
    Void        *outputData;
    const Int16 *inputPcm;
    Int16       *outputPcm;
    Void        *workBuffer;
    UInt32       byteCount;
    UInt32       frameCount;
    UInt32       sampleCount;
    UInt32       sequence;
    UInt32       workBytes;
    UInt16       channels;
    UInt16       bitDepth;
    UInt32       sampleRate;
    UInt32       flags;
    UInt32       reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoFrame;
```

重要字段：

| 字段 | 含义 |
|---|---|
| `inputData` | 输入数据通用指针 |
| `outputData` | 输出数据通用指针 |
| `inputPcm` | S16_LE 便捷视图 |
| `outputPcm` | S16_LE 便捷视图 |
| `byteCount` | 本次 payload 字节数 |
| `frameCount` | 本次音频帧数 |
| `sampleCount` | 本次 sample 总数 |
| `sequence` | 第几个处理块 |
| `workBuffer` | 预留临时工作区 |
| `flags` | 预留帧级控制标志 |

当前 DSP Server 设置的是：

```text
inputData == outputData
inputPcm  == outputPcm
```

也就是**原地处理**。当前推荐继续使用原地处理，简单、低延迟、少拷贝。

如果后续算法需要保留原始输入，同时写独立输出，可以升级为：

```text
inputBuffer  : ARM 写，DSP 读
outputBuffer : DSP 写，ARM 读
workBuffer   : DSP 算法临时区
```

### 算法状态

```c
typedef struct {
    UInt32 blocksProcessed;
    UInt32 framesProcessed;
    UInt32 samplesProcessed;
    UInt32 lastFrameCount;
    Int    lastStatus;
    UInt32 reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoStats;
```

用途：

- 统计 DSP 已处理 block/frame/sample 数量。
- 调试算法是否被调用、是否处理失败。

### DSP 算法入口

当前实现：

```text
AudioAlgo_process()
  validates AudioAlgoFrame
  copies inputData to outputData when they are separate
  applies ctx->params.mode for S16_LE stereo modes
  calls dsp_process_audio() for swap/gain/speed helper logic
  calls dsp_spatial_process_inplace() for spatial40/spatial60
  calls dsp_spatial_process_orbit_inplace() for orbit
  calls dsp_spatial_process_dual_inplace() for dual4060
```

约束：

- 保留函数签名不变。
- 先检查 `frame->channels / bitDepth / sampleRate`。
- 空间模式要求 44100 Hz、双声道、S16_LE；`spatial40`、`spatial60`、
  `orbit` 先下混成单声道，`dual4060` 保留左右输入作为两个独立声源。
- `DspSpatialState` 必须跨 block 保留，配置或模式变化时才清零。
- 如果算法支持更多格式，使用 `inputData/outputData`。
- 出错返回 `-1`，DSP Server 会通知 ARM `APP_CMD_OP_FAILED`。

# ARM/DSP 通信协议概要

共用定义在：

Current rule: keep `AudioAlgo_process()` as the integration entry point. New
algorithm variants should either extend `AudioAlgoParams.mode` or be called from
inside `AudioAlgo_process()` so the existing frame/config/stats contract remains
stable.

```text
shared/AppCommon.h
```

主要命令：

| 命令 | 方向 | 含义 |
|---|---|---|
| `APP_SPTR_LADDR/HADDR` | ARM -> DSP | 发送 SharedRegion 指针 |
| `APP_SPTR_ADDR_ACK` | DSP -> ARM | 指针接收确认 |
| `APP_CMD_AUDIO_CFG_RATE` | ARM -> DSP | 发送 sampleRate |
| `APP_CMD_AUDIO_CFG_FMT` | ARM -> DSP | 发送 channels/bitDepth |
| `APP_CMD_AUDIO_CFG_ACK` | DSP -> ARM | 音频配置确认 |
| `APP_CMD_PROCESSING` | ARM -> DSP | 请求处理共享 buffer |
| `APP_CMD_OP_COMPLETE` | DSP -> ARM | 处理成功 |
| `APP_CMD_OP_FAILED` | DSP -> ARM | 处理失败 |
| `APP_CMD_SHUTDOWN` | ARM -> DSP | 请求关闭 |
| `APP_CMD_SHUTDOWN_ACK` | DSP -> ARM | 关闭确认 |

限制：

- Notify payload 当前只携带 16 bit 数据字段。
- `APP_MAX_PAYLOAD_SIZE = 65535`。
- 当前 sampleRate 限制为 `<= 65535`，空间链路使用 44100 Hz。
- 若后续要支持 96000/192000，建议改为共享配置结构体或 RATE 高低 16 位拆包。

# 构建与部署指南

### 源码放置路径

建议在虚拟机中放：

```sh
~/ex04
```

目录结构应包含：

```text
~/ex04/
  host/
  dsp/
  shared/
  makefile
  products.mak
  run.sh
```

不需要复制根目录 `ALSA/` 或 `ALSA-NEW/` 参与主工程构建。

### 配置工具链路径

修改：

```text
products.mak
```

填写真实路径：

```makefile
BIOS_INSTALL_DIR
CGT_ARM_PREFIX
IPC_INSTALL_DIR
SYSLINK_INSTALL_DIR
CGT_C674_ELF_INSTALL_DIR
XDC_INSTALL_DIR
```

### 配置 ARM sysroot

如果编译时报：

```text
fatal error: alsa/asoundlib.h: No such file or directory
```

说明 ARM sysroot 没传给 host makefile，或者 sysroot 里没有 ALSA 开发头文件。

先查找：

```sh
find /home/user/ti-ezsdk_dm816x-evm_5_05_01_04 -path '*/alsa/asoundlib.h'
```

如果找到：

```text
/home/user/ti-ezsdk_dm816x-evm_5_05_01_04/linux-devkit/arm-none-linux-gnueabi/usr/include/alsa/asoundlib.h
```

则构建时传：

```sh
make ARM_SYSROOT=/home/user/ti-ezsdk_dm816x-evm_5_05_01_04/linux-devkit/arm-none-linux-gnueabi
```

如果在 rootfs 里找到，也可以临时使用 rootfs 作为 sysroot：

```sh
make ARM_SYSROOT=/home/user/ti-ezsdk_dm816x-evm_5_05_01_04/filesystem/ezsdk-dm816x-evm-rootfs
```

### 编译

在虚拟机：

```sh
cd ~/ex04
make clean
make ARM_SYSROOT=/path/to/arm/sysroot
```

成功后应生成：

```text
host/bin/debug/app_host
host/bin/release/app_host
dsp/bin/debug/server_dsp.xe674
dsp/bin/release/server_dsp.xe674
```

### 安装到 NFS rootfs

假设 ARM rootfs 的 NFS 路径是：

```text
/home/root
```

运行：

```sh
make install EXEC_DIR=/home/root
```

安装后目录：

```text
/home/root/ex04_syslink_dsp/debug/
  app_host
  server_dsp.xe674
  slaveloader
  run.sh

/home/root/ex04_syslink_dsp/release/
  app_host
  server_dsp.xe674
  slaveloader
  run.sh
```

### ARM 板端运行

进入 ARM Linux 用户空间：

```sh
cd /home/root/ex04_syslink_dsp/release
chmod +x run.sh app_host slaveloader
```

推荐测试顺序：

```sh
./run.sh smoke
./run.sh record test.wav 5
./run.sh file test.wav out.wav
./run.sh file test.wav spatial40.wav spatial40
./run.sh file test.wav spatial60.wav spatial60
./run.sh file test.wav orbit.wav orbit
./run.sh file dual_footsteps.wav dual4060.wav dual4060
./run.sh audio 5
./run.sh audio 10 spatial40
./run.sh audio 10 spatial60
./run.sh audio 10 orbit
./run.sh audio 10 dual4060
```

### 可选：部署成员 A 的 audioTest_arm

如果需要纯 ALSA 声卡单测，可以单独构建和部署 `audioTest_arm`。

它不是主工程必须项。

部署后可以运行：

```sh
./audioTest_arm record test.wav 5
./audioTest_arm play test.wav
```

用途：

- 排查声卡录音/播放是否正常
- 不经过 DSP
- 不验证 SysLink

# 当前限制和后续建议

当前限制：

- DSP input/output 当前共用同一块 SharedRegion buffer，属于原地处理。
- Notify payload 数据字段为 16 bit，单次 payload 最大 65535 字节。
- sampleRate 当前限制为 `<= 65535`。
- `file` 模式只接受和平台配置一致的 WAV。
- `audioWavReadFrames()` / `audioWavWriteFrames()` 当前接口仍是 `short *`，默认服务 S16_LE。

后续建议：

- 算法复杂后，如需保留原始输入和独立输出，可升级为 input/output/work 三 buffer。
- 若要支持 96000/192000 Hz，建议把音频配置改成 SharedRegion 配置结构体。
- 若要支持更多 WAV 格式，可在 `file` 模式增加重采样/格式转换，或放宽平台配置匹配。
- 若算法处理耗时较长，可通过构建参数覆盖：

```sh
make ARM_SYSROOT=/path/to/sysroot CFLAGS+='-DSYSLINK_DSP_WAIT_TIMEOUT_SEC=30'
```
