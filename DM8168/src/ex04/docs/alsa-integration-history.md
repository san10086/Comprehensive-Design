# ALSA 模块承接记录

### 已承接到主工程的文件

成员 A 的核心 ALSA 模块已经整合到 `host/audio/`：

| ALSA-NEW 文件 | 主工程承接位置 | 当前用途 |
|---|---|---|
| `ALSA-NEW/audioCapture.c` | `host/audio/audioCapture.c` | ALSA 录音 |
| `ALSA-NEW/audioCapture.h` | `host/audio/audioCapture.h` | 录音接口 |
| `ALSA-NEW/audioPlayback.c` | `host/audio/audioPlayback.c` | ALSA 播放 |
| `ALSA-NEW/audioPlayback.h` | `host/audio/audioPlayback.h` | 播放接口 |
| `ALSA-NEW/audioWav.c` | `host/audio/audioWav.c` | WAV 读写 |
| `ALSA-NEW/audioWav.h` | `host/audio/audioWav.h` | WAV 接口 |
| `ALSA-NEW/audioConfig.h` | `host/audio/audioConfig.h` | 音频参数宏和 `audioConfig` |
| `ALSA-NEW/audioPipeline.h` | `host/audio/audioPipeline.h` | 流水线预留接口 |

此外主工程新增了：

| 主工程文件 | 用途 |
|---|---|
| `host/audio/audioConfig.c` | 提供 `audioGetConfig()`，供 `app_host` 获取当前平台音频配置 |

### 未承接到主工程的文件

以下文件没有放入 `host/audio/`，原因是它们属于成员 A 的独立测试程序，不是主链路库代码：

| ALSA-NEW 文件 | 是否需要主工程使用 | 说明 |
|---|---:|---|
| `ALSA-NEW/mainTest.c` | 否 | 生成独立 `audioTest/audioTest_arm`，用于纯 ALSA 单测 |
| `ALSA-NEW/Makefile` | 否 | 只服务 `audioTest` 单独构建 |
| `ALSA-NEW/deploy.sh` | 否 | 只服务 `audioTest_arm` 单独部署 |
| `ALSA-NEW/README.md` | 否 | 成员 A 模块说明文档 |

### 主工程在 ALSA-NEW 基础上的增强

`host/audio/` 不是简单复制，已经做了适配和加固：

- `audioCapInit(audioConfig *cfg)` 会把 ALSA 实际协商后的 `sampleRate / periodFrames / bufferBytes` 回写到 `cfg`。
- `audioPbInit(audioConfig *cfg)` 也会回写播放侧实际参数。
- `audioPlayback.c` 增加静音预填充，降低启动播放时 underrun 的概率。
- `audioWav.c` 增加 WAV 读取校验：
  - `fmt chunkSize >= 16`
  - `fseek()` / `ftell()` 失败检查
  - RIFF chunk 奇数字节 padding 处理
  - `blockAlign` 校验
  - `byteRate` 校验
  - `dataSize % blockAlign` 校验
- `host/app/main.c` 已经直接使用 `host/audio/` 实现 `record / audio / file` 模式。

### 和成员 A 原命令的对应关系

成员 A 原命令：

```sh
./audioTest record test.wav 5
./audioTest play test.wav
./audioTest pipe 5
```

当前主工程对应关系：

| 成员 A 命令 | 当前主工程命令 | 是否经过 DSP |
|---|---|---:|
| `./audioTest record test.wav 5` | `./run.sh record test.wav 5` | 否 |
| `./audioTest play test.wav` | `./run.sh file test.wav` | 是 |
| `./audioTest pipe 5` | `./run.sh audio 5` | 是 |

说明：

- `audioTest play` 是纯 ALSA 播放，不经过 DSP。
- `run.sh file test.wav` 会走完整链路：`WAV -> ARM -> DSP -> ARM -> Playback`。
- 如果只想排查声卡，可以继续单独部署 `audioTest_arm`。
- 主工程构建不需要复制 `ALSA/` 或 `ALSA-NEW/`，只使用 `host/audio/`。


当前纯 ALSA 单测位于 `tests/audio/`；以上原交付目录名仅作来源说明。当前构建与运行以 [项目说明](../README.md) 为准。
