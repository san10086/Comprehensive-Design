# 空间音效中期汇报：本次 PC 核验记录

> 目录整理说明：下文记录原验证时的文件布局。重复音频现统一在 `tests/spatial/`，旧可执行程序已移除；两份重渲染证据保留。`verify.py` 重新运行时使用 `CC`/PATH 中的 GCC（本机 MinGW 路径作后备），输出至被忽略的 `tests/spatial/build-verification/`，并与本目录保留的音频比较，不覆盖原始证据日志。

报告日期：2026-11-01（Asia/Shanghai）。本目录由本次验证新建，未改动算法源码、原有测试或历史音频。

## 实际执行与结果

- 使用本机 MinGW GCC 13.1.0；编译参数与 `tests/spatial/Makefile` 一致：`-std=c99 -O2 -Wall -Wextra -Werror -pedantic`。两个可执行程序均编译成功，退出码为 0。
- 两组 HRIR 的纯 C Q15 自测全部通过：脉冲响应方向正确；4097 帧固定随机输入按 1、17、511、1024、73 帧循环分块后与直接卷积逐样本一致；511 帧尾部补偿正确。
- `dsp_spatial.c` 在 PC 编译运行的自测全部通过。与 Q15 参考比较，本次 `spatial40`、`spatial60`、`orbit`、`dual4060` 的最大绝对误差均为 **1 LSB**；测试设定上限分别为 2、2、3、3 LSB。该结果只对应本次测试输入及 PC 构建，不代表所有输入的误差上界。
- `orbit` 上下端点的状态更新与反向条件自测通过。程序输出中的“turns smoothly”是测试名称，不能代替连续试听或主观平滑度测评。
- 使用当前 `tests/spatial/input.wav` 重新运行原有 C 渲染器，分别以 511 帧和 1024 帧分块，输出均为 **221011 帧**（220500 + 511）。
- 两份新输出相互比较，以及分别与历史 `rendered_511.wav`、`rendered_1024.wav` 比较，均通过零容差比较：**442022 个 PCM 样本，最大绝对误差 0 LSB，均值绝对误差 0 LSB**。历史两份文件相互比较也为 0 LSB。

## 音频文件核查

- 四个 HRIR 文件均为 **44100 Hz、单声道、PCM16、512 帧**，文件解析与原有自测读取均成功。
- 两处当前 `input.wav` 均为 **44100 Hz、双声道、PCM16、220500 帧（5 秒）**。README 仍将当前 `input.wav` 描述为伪装 M4A，已与当前文件不符。
- 两处 `input_old.wav` 的文件头为 ISO BMFF `ftyp`，major brand 为 `M4A `，不是 RIFF WAV，Python WAV 解析器予以拒绝。
- 历史 `output_40e045.wav`、`output_60e250.wav` 为 **256409 帧**，与当前输入加 511 帧尾部的预期长度不一致，不宜作为当前输入的逐样本验收参考。
- 历史 `rendered_40e045.wav`、`rendered_60e250.wav`、`rendered_511.wav`、`rendered_1024.wav` 均为 **44100 Hz、双声道、PCM16、221011 帧**。本次只实际重渲染并逐样本比较 40e045 的 511/1024 分块输出。

## 文件与截图建议

- `commands.log`：完整真实命令、工作目录、标准输出和退出码。可截取 Q15/DSP 的 PASS 行及真实 `max error 1 LSB` 作为数值正确性截图；可截取 `compare PASS`、`max absolute error: 0 LSB` 作为分块不变性截图。请保留命令或文件名以说明比较对象。
- `commands.json`：上述执行记录的结构化版本。
- `audio-inspection.md`：便于阅读和截图的音频格式/长度表。
- `audio-inspection.json`：文件头、SHA-256、PCM 数据 SHA-256、采样率、声道数、帧数、极值、RMS 和满幅样本计数。
- `verify.py`：仅使用 Python 标准库调用已有 C 自测与渲染器，并检查文件；记录了本机编译器路径。它用于复现本次验证，重复执行会更新本目录内的生成文件。
- `rerendered_511.wav`、`rerendered_1024.wav`：本次新生成的结果，原有文件未覆盖。

## 验证范围

本次未连接开发板、未安装依赖、未运行 MATLAB、未进行延迟/算力性能测量、未开展耳机试听或定位准确率实验。因此，本记录支撑“PC 侧数学和分块正确性已经验证”，不能据此写成“板端实时性能已验收”“真实空间定位准确率已达标”或“MATLAB/C 逐样本比较已经通过”。
