# 工作目录整理报告

日期：2026-11-01（Asia/Shanghai）。状态：整理与回归验证完成，待用户审核；未暂存、提交或推送 Git。

本次从已有大量未提交改动的工作区开始，先保存文件大小、SHA256 和 Git 状态，再处理有证据的冗余。整理前状态见 [Git 状态快照](evidence/workspace-cleanup-2026-11-01/git-status-before.txt)，文件基线见 [baseline.json](evidence/workspace-cleanup-2026-11-01/baseline.json)。Git 当前总 diff 包含此前开发工作，不能全部归因于本次整理。

## 删除、迁移与整合

| 对象 | 处理与依据 |
|---|---|
| 停用的 `frontend/`（16 个文件） | 原 README 已明确停用；当前工程使用 `frontend_arm/`。移除历史桌面源码与设计稿，保留备份和 Git 历史。 |
| `.codex_doc_extract/`（5 个文件） | 是课程文档的提取副本；`PPT/` 中的原始课程材料完整保留。 |
| `temp/` | 完整库原样迁到 `assets/hrir/mit_kemar/`；删除与正式样本相同的 4 个 WAV 和无引用的 2 个旧生成头文件。生成器、两处 Makefile 和当前说明书同步改路径。 |
| 重复音频（5 个文件） | 删除 `assets/hrir/original/` 内 input、input_old、两份 output，以及 `tests/test_44100.wav`；均经 SHA256 确认与 `tests/spatial/` 对应文件相同。测试/UI 使用的原文件保留，MATLAB 试听脚本改用共享输入。 |
| 旧可执行程序与测试缓存 | 删除证据目录内的 3 个 EXE、旧 Qt4 构建输出、空间测试程序/渲染缓存及 Python 缓存；原始验证日志和两份重渲染证据保留。 |
| `report/.build/` | 制作脚本、页面计划、讲稿数据与验证记录迁到 `report/workflow/`，脚本引用同步更新；清理旧 draft/candidate/revision、中间预览和扫描缓存。两份备注修订输入仍保留。 |
| 汇报候选与用户成稿 | 17 页备注修订候选移到 `report/archive/notes-revision-candidate.pptx`，没有替换 `report/output/` 中用户现有的两份文件。原模板、截图及制作参考图保留。详见 [汇报目录说明](../report/README.md)。 |
| 文档入口与过时内容 | 新增 [文档索引](README.md)；根 README 的 ALSA 承接历史移到 [专门记录](alsa-integration-history.md)。修正 PCM/M4A 描述，更新 Qt 文档的阶段状态，保留原始回传时间线。 |
| `delivery/` | 保留现有归档和校验文件作为构建快照；标明其采用整理前路径，后续按新标签重新打包。新增 [交付目录说明](../delivery/README.md)。 |

按清理清单共移除 **181 个原有文件，约 324.34 MiB**，迁移 **1472 个文件**；文件级路径、大小和原 SHA256 见 [manifest.json](evidence/workspace-cleanup-2026-11-01/manifest.json)。数量不含验证时临时生成后再次清理的输出。迁移记录包含 1431 个原始 HRIR 文件。

以上是工作目录中移出的体积；恢复备份另占约 372.44 MiB，因此不表示整机磁盘净释放同等空间。

## Git 忽略规则

新增根 [.gitignore](../.gitignore)，合并 `frontend_arm/`、`tests/spatial/` 和 `tools/` 中的分散规则；停用前端自己的规则随目录删除。

- 忽略 host/DSP/Qt 构建树、安装输出、对象文件、Python/编辑器缓存及空间测试生成物。
- 忽略 `delivery/` 的压缩包及对应 SHA256/MD5 文件；查看器配置和说明继续可见。
- 忽略本机汇报模板、成稿、候选、制作输入二进制及可再生成的预览；截图、脚本、计划、讲稿和验证日志继续可见。
- 原始 HRIR、生成系数/方向目录、源码 Makefile 和 TI 必需库继续可见，未使用全局 `*.wav`、`*.a`、`*.log` 或整个 `report/` 的忽略规则。

22 个正反例检查通过，详见 [忽略规则检查](evidence/workspace-cleanup-2026-11-01/ignore-checks.json)。`.gitignore` 不会自动取消已跟踪文件；本次需要移除的旧文件已表现为删除，待用户审核提交。

## 验证结果

在现有本机 WSL Ubuntu 22.04、GCC 11、Qt 5.15.3 环境运行，无联网或安装依赖。

| 检查 | 结果与证据 |
|---|---|
| 参考卷积、DSP 兼容方向、完整 710 方向库 | 通过；变长块、尾部、双源与缓存检查通过，见 [空间测试日志](evidence/workspace-cleanup-2026-11-01/spatial-tests.log)。 |
| 生成器 5 项、协议 2 项 | 全部通过，包含原始库与生成数组一致性；同上。 |
| host 停止与参数测试 10 项 | 全部通过，见 [host 日志](evidence/workspace-cleanup-2026-11-01/host-tests.log)。 |
| Qt 生命周期/IPC 15 项、界面/波形 10 项 | 全部通过，包含保留音频样本的文件加载与 GUI 交互，见 [Qt 日志](evidence/workspace-cleanup-2026-11-01/qt-tests.log)。计数包含初始化/清理项。 |
| 清理后的验证脚本复现 | 通过；511/1024 分块结果及与保留音频证据比较均为 0 LSB，见 [复现日志](evidence/workspace-cleanup-2026-11-01/reproduce-audio.log)。新输出使用独立临时目录，原始日志未覆盖。 |
| 源码保全、备份及路径 | 60 个 host/DSP/shared/Qt 功能文件与整理前 SHA256 一致；删除项均能从备份按原 SHA256 恢复，本地 Markdown 文件链接通过检查。见 [完整性检查](evidence/workspace-cleanup-2026-11-01/integrity-checks.json)。 |
| 源码打包说明实测 | 使用更新后的打包命令生成临时归档并回读，1431 个原始 HRIR WAV 全部包含；构建缓存排除，构建脚本与根 `.gitignore` 包含，临时归档已清理。同上。 |

本次未重新运行目标 ARM/TI 交叉构建、MATLAB 或板端硬件验收。已有目标构建记录保留；桌面回归不代表真实音频、HDMI 或显示/DSP 共存已通过。

## 恢复与审核

恢复备份位于本机：[workspace-cleanup-backup-20261101.zip](C:/Users/30522/.codex/visualizations/2026/11/01/01a196fe-21ad-7dd2-af5f-3817f29bdff0/workspace-cleanup-backup-20261101.zip)。备份覆盖本次移除的原文件和整理前完整汇报制作目录，SHA256 记录在 `manifest.json`。恢复时按清单选择文件提取；不要整包覆盖整理后的目录。

本次对已有文件的改动路径及前后 SHA256 单列在 [changes-from-baseline.json](evidence/workspace-cleanup-2026-11-01/changes-from-baseline.json)，其中包括回归验证重新生成的桌面构建日志/截图。原 Git diff 中的空间汇报稿已有 Markdown 尾空格/末尾空行提示，该稿内容与整理前一致，本次未改写其排版。

请重点审核：停用桌面前端的移除、HRIR 新路径、音频去重位置，以及是否采用单独保留的备注修订候选稿。现有成稿、历史包、课程原件和功能代码的既有改动均保留。
