# MIT KEMAR 方位库更新：本次复核记录

> 后续结果：本页保留当时 MinGW 的成功与失败记录；后续用户 Ubuntu20 回传已包含完整库、协议与 host 测试通过结果，见 [构建回传](../arm-build-2026-11-01/README.md)。原始库现位于 `assets/hrir/mit_kemar/`，本目录旧可执行程序已清理，日志保留。

报告日期：2026-11-01，Asia/Shanghai。此次仅更新汇报稿件并复核现有实现，未改算法、导入器或测试源码，未连接目标板。

## 通过的检查

- `python tools/generate_hrir.py --check` 通过。1420 个方向 WAV，710 组双耳响应，14 层仰角（−40° 至 90°）。忽略 11 个非方向 WAV，原始 PCM16 系数合计 1,454,080 字节。
- `python tests/spatial/test_hrir_generator.py`：5 项测试全部通过，包括全库原始 PCM16/生成数组等价、负仰角与确定性、错误输入保留已有生成结果、缺耳/重复/角度校验、损坏或截断 WAV。
- 使用本机 MinGW GCC 13.1.0，按 `-std=c99 -O2 -Wall -Wextra -Werror -pedantic` 编译当前 `dsp_spatial_selftest.c`、`spatial_fir.c`、`wav_pcm16.c`、`dsp/dsp_spatial.c`，成功退出。
- 实际运行新编译程序的 `--catalog`，710 个方向全部通过与 Q15 直接卷积的数值比较，覆盖变长块、511 帧尾部与代表性双源组合。单方向接受阈值为 2 LSB，双源为 3 LSB，属于该测试输入和构建环境的验证范围。
- 缓存切换与非法方向自测通过。原兼容方向、orbit、固定双源本次显示的最大误差均为 1 LSB，饱和/混合后削波测试与 orbit 端点逻辑测试通过。此结论不构成任意输入误差上界或听感评价。

真实命令与输出见 `import-tests.log`、`dsp-catalog-tests.log`，方向数量与仰角分布见 `catalog-summary.json`。编译时将 GCC bin 目录加入进程 PATH，以找到编译器所需 DLL。

## 未完成的检查

`test_spatial_protocol.py` 在当前 MinGW 下的两个测试均在链接阶段出现未解析的 TI 模拟符号（例如 Memory_free、Notify_unregisterEvent、Semaphore_post），测试逻辑未执行，不能记为 PASS。原始输出保存在 `protocol-tests.log`。

另作一次保持测试源文件不变、仅为编译命令加入 `-flto` 的构建尝试，本机 GCC 未启用 LTO 支持，仍未执行测试，记录见 `protocol-tests-mingw-lto.log`。应在 Ubuntu/POSIX 开发环境重新运行原有协议及 host CLI 测试。

此次没有完成目标 TI 工具链的最终链接、map 文件检查、硬件共享内存协议验收、ALSA 连续运行、延迟测量或耳机试听。

## 汇报截图

仅从成功的导入及算法日志中截取 710 方向 PASS 与缓存检查结果，并注明本机验证环境。协议部分以“实现和模拟测试已接入，待 Linux 复核”表述，不能使用未执行的预期 PASS。
