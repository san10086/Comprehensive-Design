# 汇报资料

| 位置 | 用途 |
|---|---|
| `template.pptx` | 原始模板，保留本机文件 |
| `output/` | 用户现有的两份汇报文件，保持原件 |
| `ima/` | 用户截图及从制作目录保留的 `authoring-reference.jpg` |
| `workflow/` | 从 `.build/` 整合的制作脚本、页面计划、讲稿数据和验证记录 |
| `archive/notes-revision-candidate.pptx` | 单独保留的 17 页备注修订候选，SHA256 `1f035442bf54bd9beb06e321b987f7906b521d2b3d91c3c0a5fd6068604ee329` |

候选稿与当前 `output/中期汇报.pptx` 内容不同，本次未替换用户成稿；是否采用由用户审核。候选来自原 `notes-revision/candidate.pptx`，其重复的 `output/validated.pptx` 已清理。备注修订的 `source.pptx` 和 `artifact-notes.pptx` 作为制作输入保留在 `workflow/notes-revision/`。

旧 draft、candidate、revision 等中间版本、预览图片和重复扫描缓存已移出工作目录，清理前完整制作目录已备份。恢复位置见 [整理报告](../docs/workspace-cleanup-2026-11-01.md)。制作脚本中的工作路径已改为 `report/workflow/`；脚本仍依赖原本机 Artifact Tool 路径和对应阶段输入，属于历史制作工具，执行前先确认环境与输出目标，不能当作通用一键构建流程。

二进制模板、成稿、候选稿、制作输入和可再生成的预览通过根 `.gitignore` 保留在本机；截图、脚本、计划、讲稿及验证记录保持可见，供 Git 审核。不要把 `report/` 整体忽略。
