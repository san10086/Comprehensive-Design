# Windows VNC 兼容转发

本次 Qt5.9.7 的 GUI 已能在板端绘制中文和波形，但 TigerVNC 1.16.2 直接连接时发生黑屏，固定 256 色参数也未能稳定解决。本机转发在握手后显式发送服务端像素格式，再转发查看器数据。用户已确认全彩中文、波形，以及正常关闭查看器后重新连接均正常。

## 日常启动

1. 板端先运行 GUI：

   ```sh
   sh /home/root/ex04-fonts/run-gui-vnc.sh > /tmp/ex04-gui.log 2>&1 < /dev/null &
   ```

2. Windows 双击本目录 `start-vnc.cmd`。它使用已有的 Python 3 启动后台转发，再打开 TigerVNC。重复启动会复用已有转发，不会重复占用 5901。

本机实际查看器为 `E:\课程资料\综设Ⅱ\vncviewer64-1.16.2.exe`。更换安装位置时修改 `start-vnc.ps1` 的 `ViewerPath` 默认值；板端 IP 改变时修改 `vnc-qt597-bridge.py` 的 `--target` 默认值。

查看器连接 **127.0.0.1::5901**，转发连接 **192.168.1.21:5900**。`FullColor=1`、`AutoSelect=0`、`PreferredEncoding=Raw`、`Shared=1` 已由启动脚本设置。启动脚本会打开可见查看器，转发进程在后台运行。

转发已运行时，也可使用 [保存的查看器配置](../../delivery/ex04-vnc-via-bridge.tigervnc)。直接打开配置不会自行启动转发。

## 排错及范围

- 转发日志：`frontend_arm/build-desktop/vnc-bridge.log`，错误日志：同目录 `vnc-bridge-error.log`。日志仅记录协议格式、编码类型和数据字节数，不记录键盘或剪贴板内容。
- 板端 GUI 退出后，需要重新启动 GUI，再打开查看器；本机转发不会自动启动板端进程。
- `PORT_BUSY=5901` 表示存在无法由当前 PID 文件识别的监听程序。核对本机进程后处理，不对未知进程自动结束。
- 这次没有重编译或修补板端 Qt 插件。转发仅支持已验证的 RFB 3.3、None 安全类型和真彩格式，且只监听本机回环地址。不要用纯 TCP 探测直接测试旧 Qt VNC 插件。

验证包括：本机模拟服务的分片握手、格式初始化顺序和双向数据完整性；实际板端完整画面和中文；用户确认全彩界面正常；关闭查看器、改用正式启动脚本后的重连再次正常。记录见 [板端验证](../../docs/evidence/arm-build-2026-11-01/README.md)。
