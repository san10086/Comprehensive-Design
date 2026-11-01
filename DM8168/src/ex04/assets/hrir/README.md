# HRIR 数据与音频样本

- `mit_kemar/`：完整原始库，从原 `temp/MIT KERMER` 原样迁入。1420 个方向 WAV 组成 710 组，另有 11 个非方向响应 WAV；生成器会忽略非方向文件。数据不是临时缓存，必须随源码保存。
- `original/`：早期 MATLAB 试听脚本与四个兼容方向 HRIR。这四个小样本有意独立保留，用于参考卷积和生成器的最小测试，不要求测试依赖整个原始库。
- 输入、旧格式反例和两份历史试听结果统一位于 `tests/spatial/`。原 `assets/hrir/original/` 的同名音频以及 `tests/test_44100.wav` 经 SHA256 核对完全重复，已去重。

在项目根目录运行 `python tools/generate_hrir.py --check` 可核对原始库与已生成的 `shared/hrir_catalog.h`、`dsp/dsp_spatial_coefficients.inc`。生成文件是当前构建输入，继续纳入版本管理。

MATLAB 试听脚本仍从 `original/` 目录执行，输入指向共享测试样本，输出写入 `tests/spatial/rendered_matlab_40e045.wav`，避免覆盖历史试听样本。精确 Q15 对照使用 [空间测试说明](../../tests/spatial/README.md) 中的独立脚本。
