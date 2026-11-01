# 用户 Ubuntu20 构建回传（2026-11-01）

以下原始日志由用户在其 Ubuntu 虚拟机执行后回传，本机仅归档与核对，未代为连接该虚拟机。

- [源码测试](source-tests-user.log)：HRIR 710 方向/1420 WAV 校验、参考卷积、完整目录 FFT/dual、缓存及拒绝路径通过；Python 生成器 5 项、协议 2 项、host 停止/参数 10 项通过。
- [host/DSP 构建](host-dsp-build-user.log)：Sourcery GCC 4.8.3 与 EZSDK sysroot 完成 debug/release host；XDC 3.23.03.53、TI C674 CGT 7.3.1 完成 debug/release DSP。
- [Qt5.9.7 ARM 构建](qt597-arm-build-user.log)：linux-TIarmv7-g++、ARMv7-A/Cortex-A8/NEON/softfp；GUI 和 IPC peer 均为 32 位 ARM EABI5，解释器为 /lib/ld-linux.so.3。
- [发布包构建](package-build-user.log)：显示探针为 ARM EABI5；host/DSP/Qt 安装完成，release 内 11 个文件的 SHA256 清单复核全部 OK，部署包已生成。

用户虚拟机生成的部署包为 `/home/user/ex04-work/ex04-source-20261101-ui/delivery/ex04-dm8168-qt597-20261101-ui.tar.gz`，SHA256 为 `3f8778a4c10d5c6813580b5a202b6be3d2d0c114a57f6c4c337b0a03a5901a0b`。该包位于用户虚拟机，本机未持有或重新生成这份 ARM 发布包。回传的虚拟机地址为 `192.168.198.130` 和 `192.168.1.41`；板端下载使用与板子同网段的 `192.168.1.41`，地址改变后需重新查询。

GUI 动态依赖为 Qt5Widgets/Qt5Gui/Qt5Core 及 ARM C/C++ 运行库，RPATH 为板端已有的 /home/qt_arm/qt5.9.7_install/lib。

板端首次部署在前置检查时返回。随后用户明确回传 `-sh: command: not found` 和 `sha256sum: applet not found`：前者使旧版 `command -v` 探测失效，不能据此断言 wget / MD5 不存在；后者仅确认 BusyBox 未内置 SHA256。现采用 [部署辅助脚本](../../../tools/deploy/README.md) 实际运行工具选择校验方式。本机 WSL 的 [8 项隔离验证输出](legacy-deploy-tests.txt) 与目标板结果分开记录。

用户随后在会话中确认辅助流程“成功了”。据此记录板端文件部署已获用户确认；尚未取得完整部署输出，实际选用的哈希工具、备份路径和版本输出待补充归档。下一步为新版 GUI 的板端显示与交互验收；这项部署确认不代表 GUI、VNC 稳定性或音频/DSP 运行已经通过。

用户继续回传 [TigerVNC 黑屏截图](gui-vnc-black-user.png) 和 [板端诊断日志](gui-black-diagnostics-user.log)，GUI 日志反复出现 `Cannot handle 0 bpp client`，5900 仍在监听。随后本机完成一次完整 RFB 3.3 握手，显式发送 32 位像素格式，取得真实板端的 [800×480 GUI 帧](gui-rfb-frame.png)：蓝白界面及四条波形已绘制，中文显示方块。这证明该时刻 GUI 已完成首帧绘制，不代表持续交互或音频通过。

本机随后以 `-AutoSelect=0 -FullColor=0 -LowColorLevel=2 -PreferredEncoding=Raw -Shared=1` 打开 TigerVNC 1.16.2，用户明确回复“已显示，但中文是方块”。据此记录默认连接黑屏已通过固定 256 色参数绕过；尚未修改或重编译目标 Qt 插件。Qt5.9.7 服务端像素格式初始化及 TigerVNC 跳过同格式设置的路径已核对官方源码，见 [Qt 源码](https://github.com/qt/qtbase/blob/v5.9.7/src/plugins/platforms/vnc/qvncclient.cpp) 和 [TigerVNC 源码](https://github.com/TigerVNC/tigervnc/blob/v1.16.2/common/rfb/CConnection.cxx)。

已准备 [中文字库补充包](../../../delivery/ex04-cjk-fonts-20261101.tar.gz)，包含原始文泉驿微米黑、许可证、字体配置及启动脚本，未改变 ARM 业务发布包。包内文件与归档回读一致、字体 TTC 标识及配置 XML 检查通过，并提供 SHA256 / MD5 两种清单；SHA256 为 `b72df21043c4c17156198e7dba80cad95fd8403e84e5a4da5e0eaeb267f3d38d`。安装步骤见 [说明书 9.4](../../空间处理与Qt更新编译打包说明书_XFTP.md#94-安装中文字库并重启-gui)。

用户安装重启后回传 GUI 已就绪、VNC 5900 已创建，但查看器再次黑屏。本机重新完成完整 RFB 握手并显式发送像素格式，分别取得 [32 位画面](gui-rfb-after-fonts-frame.png) 和 [256 色画面](gui-rfb-after-fonts-8bpp-frame.png)：两种格式都显示完整中文、导航和四条波形，字库修复已由实际板端帧确认。黑屏时 Windows 查看器进程确实带上述固定色彩参数，且 TCP 连接仍为 ESTABLISHED，因此不能仅凭“参数正确”认定该查看器会话已经可用。已重新打开查看器，等待用户确认新窗口状态；当前证据支持板端绘制及显式像素格式的帧传输正常，尚不能确定旧查看器会话黑屏的具体原因。

随后新窗口报错，本机完整握手连接返回 WinError 10061；用户回传 `ps` 没有 GUI 进程，现有日志未给出退出码，内核尾部也没有该次崩溃证据。不能据此把退出原因确定为段错误。用户使用带退出码记录的后台包装重新启动 GUI 后，本机加上 [RFB 兼容转发](../../../tools/deploy/VNC_BRIDGE.md)，在转发 ServerInit 前向 Qt 显式发送其公布的 32 位像素格式。查看器用全彩连接 `127.0.0.1::5901`，用户明确确认“中文和波形均正常”。

之后正常关闭该查看器，把临时转发换成正式 Windows 后台进程，并实际执行 `start-vnc.cmd` 使用的 Windows PowerShell 5.1 启动脚本。重连再次收到帧更新，用户明确确认“中文和波形仍正常”。[正式转发日志](gui-vnc-bridge.log) 记录 32 位格式初始化、TigerVNC 编码协商及更新请求；未记录输入内容。转发的分片握手、像素格式初始化顺序及双向数据完整性也已用本机模拟服务验证。未重编译目标 Qt 插件，原字体包、源码包和 ARM 发布包均保持原件。

DSP map 的 DSP_PROG 起点为 0x8f000000，长度 0x01000000；used 为 0x001b60e6（1,794,278 字节，约 1.71 MiB），unused 为 0x00e49f1a（约 14.29 MiB）。入口符号 ti_sysbios_family_c64p_Hwi0 与 .vecs 起点均为 0x8f1b6000。SR0/SR1 的完整占用在日志中为 NOLOAD 预留段，不代表相同大小的运行时音频缓冲使用量。

构建仍有 DSP .plt/入口符号警告和 C IPC peer 的特性宏警告，均未导致构建失败。目前源码测试、交叉编译、所示链接布局、GUI 板端中文及波形绘制，以及通过本机兼容转发的一次断开重连已确认；长期运行、真实音频和显示共存仍需分别验收。
