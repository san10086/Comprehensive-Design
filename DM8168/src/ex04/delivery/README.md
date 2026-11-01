# 本机交付文件

本目录保留 2026-11-01 的源码包、部署辅助包、中文字库包及各自 SHA256 文件。这些包是当时构建与传输的快照，本次不重打包或改写原校验值；压缩包和校验侧文件由根 `.gitignore` 排除版本管理。

源码包内采用整理前的 `temp/MIT KERMER` 路径。当前源码已改为 `assets/hrir/mit_kemar/`，后续更新按 [XFTP 说明书第 2.2 节](../docs/空间处理与Qt更新编译打包说明书_XFTP.md#22-以后如何重新打包) 生成新的时间标签，不覆盖历史包。

`ex04-vnc-via-bridge.tigervnc` 是当前兼容转发连接配置；`ex04-vnc-compat.tigervnc` 是此前固定色彩排查配置，保留用于复现历史问题。配置文件继续纳入版本管理。日常连接使用 [VNC 启动说明](../tools/deploy/VNC_BRIDGE.md)。
