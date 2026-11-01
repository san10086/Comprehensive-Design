# DM8168 发布包部署辅助脚本

适用本次 Ubuntu20 构建结果与精简板端 shell。脚本不依赖板端的 `command`、`type` 或 `which`，通过执行空文件哈希选择独立 SHA256、BusyBox SHA256、独立 MD5 或 BusyBox MD5。

`prepare-checksums.sh` 在 Ubuntu Bash 运行，读取 `$HOME/ex04-build-paths.sh`。它先比较归档内和暂存目录的 SHA256 清单、验证暂存文件，再生成发布包旁的 `.sha256`、`.md5` 和 `MD5SUMS`。现有 ARM 归档不变；MD5 用于精简板端的传输与文件完整性核对。

`deploy-board.sh` 在板端以子进程运行，参数为 Ubuntu IP 和源码标签，默认 `192.168.1.41 20261101-ui`。它检查项目进程，下载并核对归档及文件，备份旧 release，复制并复核安装结果，最后输出 `DEPLOY_OK=目录`。脚本不会启动 GUI、音频或加载 DSP。失败保留本次下载和日志输出；发生复制或安装后校验失败时，旧版本备份仍保留，需据输出处理后再运行项目。

使用说明见 [XFTP 编译部署说明书第 8 节](../../docs/空间处理与Qt更新编译打包说明书_XFTP.md#8-虚拟机--板端上传校验备份更新)。Windows 生成的辅助包 `delivery/ex04-board-deploy-20261101.tar.gz` 以 `board-deploy/` 为顶层目录，包含本目录两份脚本和本说明。解压到 Ubuntu 的 `$PROJECT_ROOT/delivery` 后由现有 8000 端口文件服务提供板端脚本及校验文件。

脚本内的 `exit 1` 只终止通过 `bash 脚本` / `sh 脚本` 启动的子进程；不要将脚本正文粘贴进交互终端，也不要 source 板端部署脚本。

本机 WSL 临时目录验证通过：SHA256、独立 MD5、BusyBox MD5、工具均不可用、归档损坏、文件清单不匹配、已有项目进程、暂存目录变更。测试覆盖备份内容、用户 WAV 保留、可执行权限和失败时不覆盖旧程序；实际板端结果另行验收。`EX04_DEPLOY_ROOT` 允许测试时选用独立临时目录，默认板端部署根目录为 `/home/root`。
