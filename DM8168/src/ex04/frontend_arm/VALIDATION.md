# 验证记录

2026-11-01 最新板端回传：用户已确认辅助部署成功，GUI 经 Windows RFB 兼容转发可显示完整中文、波形，并完成一次断开重连。详见 [回传原始记录](../docs/evidence/arm-build-2026-11-01/README.md)。下表保留各轮测试当时的结果；真实音频、长期运行、HDMI/鼠标和显示/DSP 共存仍未验收。

## Qt 界面重设计（2026-11-01）

| 检查 | 结果 | 实际环境与边界 |
|---|---|---|
| Qt5 桌面 GUI 与 IPC peer 完整构建 | 通过 | WSL Ubuntu 22.04、GCC 11、Qt 5.15.3，x86-64 ELF |
| 进程生命周期及 IPC QtTest | 15 通过、0 失败 | 新版具名参数、特殊字符路径、文件锁、启动/运行/卸载失败、协作停止、真实 System V IPC |
| 参数、波形及 GUI QtTest | 10 通过、0 失败 | 710 个方向、拒绝旧标识、WAV 格式及损坏校验、包络包含全部采样、精确游标值、过期请求、固定容量、演示与真实文件交互 |
| 800×480 / 1024×768 四页截图 | 通过 | 实际 Qt Widgets + offscreen，中文字体；已检查波形、参数、诊断、设置及小屏双声源、录音、错误状态 |
| 用户 Ubuntu20 host / DSP debug 与 release 构建 | 用户回传通过 | ARM GCC 4.8.3、EZSDK sysroot、XDC 3.23、C674 CGT 7.3.1 |
| 用户 Ubuntu20 Qt5.9.7 ARM GUI / IPC peer 构建 | 用户回传通过 | 两个产物均为 ARM EABI5；GUI 链接 Qt5，解释器 /lib/ld-linux.so.3 |
| 显示探针与 ARM 发布包 | 用户回传通过 | 探针为 ARM EABI5；release 共 11 个文件 SHA256 复核 OK；尚未代表板端部署或运行通过 |
| 精简 shell 部署辅助脚本 | 本机隔离验证 8 项通过 | 模拟 command 不可用，覆盖 SHA256 / MD5 / BusyBox MD5、缺工具、损坏归档/清单、运行中拒绝、暂存变更；非板端运行结果 |
| DSP 程序区链接布局 | 用户回传通过 | DSP_PROG 使用 0x001b60e6（约 1.71 MiB），配置 16 MiB；非运行期峰值验收 |
| Qt4.6 完整构建 | 未执行 | 保留共用 API / C++98 写法，尚未取得对应完整构建结果 |
| 真实实时 PCM、ALSA/SysLink 与 HDMI | 未执行 | 本轮提供接入接口；桌面演示和截图不代表板端硬件通过 |

截图与原始测试输出见 [UI 验收证据](../docs/evidence/frontend-ui-2026-11-01/README.md)。
用户虚拟机的源码测试、host/DSP 和 Qt5.9.7 ARM 构建原始输出见
[Ubuntu20 构建回传](../docs/evidence/arm-build-2026-11-01/README.md)，与下面的本机桌面 SDK 分开记录。
Qt5 SDK 使用官方 Ubuntu 包独立解压到本机开发缓存目录，避开该 WSL 原有未完成的
MySQL 包安装；未将本机 SDK 或桌面 Qt 库写入源码和部署包。

本轮还修复了回归测试发现的 QProcess 重入：`FailedToStart` 回调中不再立即
复用同一进程对象，改在下一个事件循环执行卸载，确保失败路径完成清理和释放文件锁。

## 先前验证（2026-10-28）

| 检查 | 结果 | 实际环境与边界 |
|---|---|---|
| C 辅助进程编译 | 通过 | Ubuntu WSL，GCC，C99，`-Wall -Wextra -Werror` |
| System V IPC 集成测试 | 6 项通过 | 真实信号量与共享内存；握手、数据、不覆盖、停止、资源移除、参数错误 |
| host 停止回归测试 | 5 项通过 | 编译实际 `host/app/main.c`，模拟音频/SysLink 实现；包括四种模式的启动期停止 |
| 构建/安装/启动脚本语法 | 通过 | Linux `/bin/sh -n` |
| 构建与安装流程 | 通过 | 模拟构建工具，实际验证 x86 ELF 被 ARM 模式拒绝、桌面覆盖选项及含空格路径的安装 |
| GUI C++ 静态语法 | 通过 | Linux G++ 配合本机已有 Qt6.4 头文件，`-fsyntax-only`；仅为补充检查 |
| Qt4.6 / Qt5 完整构建与 Qt 集成测试 | 未执行 | 当前可用 Linux 环境缺少对应 Qt 开发包；源码及测试工程已提供 |
| ARM 交叉编译、ABI/运行库匹配 | 未执行 | 未取得与板端配套的 ARM Qt SDK |
| HDMI、鼠标、音频与显示固件共存 | 未执行 | 当前未连接 DM8168；按部署说明与现有共存方案验收 |

本机用于验证的 WSL 是 Ubuntu22.04，与用户用于交叉编译的 Ubuntu20 虚拟机
不是同一环境。本机 Linux 测试和 Qt6 头文件检查不构成 Qt4.6/Qt5、ARM
或实际 ALSA/SysLink 硬件运行结果。
