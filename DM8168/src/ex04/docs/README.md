# 文档索引

当前主线是 ARM 板端 `frontend_arm/`、host/DSP 音频链路与完整 MIT KEMAR 方位库。文档按用途查阅，历史回传保留原始结果和当时路径。

| 用途 | 入口 | 内容边界 |
|---|---|---|
| 项目结构、命令和算法 | [项目 README](../README.md) | 当前源码接口 |
| 更新、交叉编译、打包、部署 | [XFTP 操作说明书](空间处理与Qt更新编译打包说明书_XFTP.md) | 当前操作主线；整理后源码使用新标签打包 |
| 环境配置原理与排查 | [环境配置报告](arm-qt-environment-setup-report.md) | 三端、SDK、显示与内存；开头说明最新状态 |
| 无屏验证与早期探针排查 | [无屏验证路径](arm-qt-headless-validation.md) | 包含 Qt4/Qt5 早期历史，不能替代最新回传 |
| 复现独立 VNC 探针 | [Qt5 VNC 探针步骤](qt5-vnc-probe-runbook.md) | 排查用；业务 GUI 部署按 XFTP 说明书 |
| Windows 查看器兼容转发 | [VNC 转发说明](../tools/deploy/VNC_BRIDGE.md) | 日常启动与连接 |
| Qt 界面、IPC 与测试 | [ARM Qt README](../frontend_arm/README.md)、[验证记录](../frontend_arm/VALIDATION.md) | 区分桌面测试与目标板结果 |
| 显示与 DSP 共存验收 | [共存测试方案](qt-syslink-coexistence-test.md) | 仍需真实硬件执行 |
| 空间算法中期汇报第一章 | [空间音效稿](中期汇报_空间音效处理算法研究与实现.md) | 研究过程与数值验证 |
| Qt 开发与后续工作汇报 | [第二、三章稿](中期汇报_第二章与第三章_Qt开发与后续工作.md) | GUI、VNC、IPC 与 RPE 后续计划 |
| ALSA 模块来源 | [承接记录](alsa-integration-history.md) | 从根 README 移出的历史交付映射 |
| 汇报文件与制作资料 | [report 说明](../report/README.md) | 模板、成稿、候选修订与制作脚本 |
| 工作目录整理 | [整理报告](workspace-cleanup-2026-11-01.md) | 删除、迁移、忽略规则及审核依据 |

最新状态证据是 [Ubuntu/板端回传](evidence/arm-build-2026-11-01/README.md)：构建、GUI 中文和波形及一次重连已有回传，长期运行、真实音频、HDMI/鼠标和显示/DSP 共存仍待验收。

算法证据分别见 [早期兼容方向验证](evidence/spatial-midterm-2026-11-01/README.md)、[完整方位库复核](evidence/spatial-kemar-update-2026-11-01/README.md)；完整库的后续 Linux 测试结果见上述 Ubuntu 回传。原始日志不因后续成功而改写。界面截图见 [UI 证据](evidence/frontend-ui-2026-11-01/README.md)。
