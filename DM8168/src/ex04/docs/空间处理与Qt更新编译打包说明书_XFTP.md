# 空间处理与 Qt 更新、编译、打包说明书（XFTP）

日期：2026-11-01。适用本仓库当前的新版空间参数和 `frontend_arm` 蓝白界面。

本文主线：**Windows 源码打包 → XFTP 上传 Ubuntu20 → 编译 ARM host / DSP / ARM Qt5 → 打包发布目录 → 传到 DM8168 → 校验并运行。**

**交互终端执行约定（修订）：**下面每段 Ubuntu/板端命令用临时函数 `ex04_step` 包裹。
失败时 `return 1` 只结束本段，保留终端和错误输出；路径变量及当前目录仍留在当前会话。
请连同函数开头和结尾一起复制。看到“本段失败”就停止，不继续下一段。
旧版段落中的裸 `exit 1` 会关闭当前 Bash，不再直接粘贴执行。

若第 3 节曾自动关闭窗口，重新开终端先按该节末尾的只读诊断检查；目标目录已存在
可能是此前已解压或解压中断，不删除、不覆盖它，也不直接认定升级已经完成。

## 1. 当前从哪里开始

| 项目 | 当前路径或状态 |
|---|---|
| Windows 工程 | `D:\develop\vscode\projects\ex04` |
| Ubuntu 用户与 EZSDK | `/home/user`；`/home/user/ti-ezsdk_dm816x-evm_5_05_01_04` |
| ARM 工具链 | `/home/user/arm-2014.05/bin/arm-none-linux-gnueabi-` |
| Ubuntu 中的 ARM Qt5 SDK | `$EZSDK/filesystem/ezsdk-dm816x-evm-rootfs/home/qt_arm/qt5.9.7_install` |
| Qt5 内嵌安装前缀 | `/home/qt_arm/qt5.9.7_install`；Ubuntu 需要建立到上面目录的路径映射 |
| 板端 Qt5 | `/home/qt_arm/qt5.9.7_install`；已有 ARM Qt5.9.7 库和 VNC 插件 |
| 板端与虚拟机地址 | 板端 `192.168.1.21`；本次回传的虚拟机同网段地址为 `192.168.1.41`，变更后用 `hostname -I` 重查 |
| 板端业务目录 | `/home/root/ex04_syslink_dsp/release` |
| 构建进展 | 用户 Ubuntu20 已通过源码测试、host/DSP debug/release、Qt5.9.7 ARM GUI 与 IPC peer 构建 |
| 发布包进展 | 显示探针 ARM 构建通过；release 11 个文件 SHA256 复核全部 OK，ARM 部署包已生成 |
| 板端 shell 兼容性 | 用户已确认 `command` 不存在、BusyBox 未内置 SHA256；第 8 节改为实际执行工具探测，支持 MD5 校验 |
| 板端部署进展 | 用户确认辅助流程成功；新版 GUI 已显示，完整部署日志待归档 |
| 显示验证 | 已安装中文字库；使用 Windows 本机兼容转发后，用户确认全彩中文、波形和一次断开重连均正常；真实音频及长期运行待验收 |

这些路径来自仓库内最新的用户环境回传。用户已按本手册完成的构建见 [原始回传记录](evidence/arm-build-2026-11-01/README.md)。本文未连接你的虚拟机或板子代为编译；本机通过的 Qt5.15.3 桌面测试，与目标 Qt5.9.7 / ARM 验收分开记录。

先认清三种包：

- **源码包**：Windows 打包，经 XFTP 上传 Ubuntu，用于编译；不能直接当作板端程序运行。
- **桌面预览版**：Ubuntu 的 `/usr/bin/qmake` 编译，运行在 Ubuntu 图形桌面；不能复制到 ARM 板运行。
- **ARM 发布包**：包含配套 `app_host`、`server_dsp.xe674`、Qt GUI、IPC peer 和脚本，部署到板端。

本次首轮升级应把 **host、DSP 和 Qt 三者一起重新编译并部署**，保证参数协议、HRIR 目录和界面一致。后续只有纯界面改动时可只更新 GUI，见第 11 节。

## 2. Windows：打包当前源码，经 XFTP 上传

### 2.1 本次现成的源码包

该包是整理前的构建快照，包内仍采用旧的 HRIR 路径。整理后的当前源码请按第 2.2 节生成新标签归档，不覆盖旧包或沿用旧校验值。

本次历史包名为 `ex04-source-20261101-ui.tar.gz`，校验文件为 `ex04-source-20261101-ui.tar.gz.sha256`，都放在工程的 `delivery` 目录。XFTP 使用现有的 Ubuntu 会话，把这两个文件以**二进制方式**上传到：

```text
/home/user/upload/
```

Ubuntu 第一次接收前执行：

```bash
ex04_step() {
# [Ubuntu20；普通 Bash]
mkdir -p "$HOME/upload"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

不要把 Windows 的 `frontend_arm/build-desktop` 编译产物传到板端；不要只上传 `frontend_arm`，新版 Qt 还依赖 `shared/hrir_catalog.h`。

### 2.2 以后如何重新打包

Windows PowerShell 执行下列命令。每次使用新时间标签，包会包含当前工作区的修改和新增文件，不依赖是否提交 Git。

```powershell
# [Windows PowerShell]
Set-Location 'D:\develop\vscode\projects\ex04'
$Tag = Get-Date -Format 'yyyyMMdd-HHmmss'
$OutDir = Join-Path $PWD 'delivery'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Archive = Join-Path $OutDir ('ex04-source-{0}.tar.gz' -f $Tag)

tar -czf $Archive --exclude='frontend_arm/build' --exclude='frontend_arm/build-*' --exclude='frontend_arm/bin' --exclude='host/bin' --exclude='dsp/bin' --exclude='dsp/configuro' --exclude='__pycache__' --exclude='*.o' --exclude='*.dep' --exclude='*.exe' --exclude='tests/spatial/spatial_q15' --exclude='tests/spatial/dsp_spatial_selftest' --exclude='tests/spatial/build-verification' --exclude='tests/spatial/rendered_*.wav' --exclude='tests/spatial/input_pcm16_44100.wav' --exclude='tests/spatial/matlab_q15_*.wav' .gitignore README.md makefile products.mak run.sh host dsp shared frontend_arm tools tests/spatial tests/qt_syslink_probe assets/hrir/original assets/hrir/mit_kemar assets/hrir/README.md docs
if ($LASTEXITCODE -ne 0) { throw '源码打包失败' }
$Digest = Get-FileHash -LiteralPath $Archive -Algorithm SHA256
$ChecksumLine = '{0}  {1}' -f $Digest.Hash.ToLowerInvariant(), ([IO.Path]::GetFileName($Archive))
[IO.File]::WriteAllText(($Archive + '.sha256'), ($ChecksumLine + "`n"), [Text.Encoding]::ASCII)
Write-Output $Archive
Write-Output ('SOURCE_TAG={0}' -f $Tag)
```

校验文件通过 WriteAllText 显式使用 LF 换行，避免 Windows CRLF 中的 `\r` 被 Linux 当作文件名的一部分。不要改回会自动追加 CRLF 的 Set-Content 写法。

XFTP 上传新 `.tar.gz` 和 `.sha256`，并将下一节的 `SOURCE_TAG` 改为刚才输出的标签。包内保留原始 HRIR 库以便重新生成；排除了 `.git`、历史桌面前端、PPT、report、已有编译目录及 SDK。

## 3. Ubuntu：保存路径，解压到新的源码目录

下面配置只创建项目的路径记录，不加载 Qt4/EZSDK 的旧环境。当前现成包的标签是 `20261101-ui`；使用自行打包的版本时改第一行标签即可。

```bash
ex04_step() {
# [Ubuntu20；普通 Bash]
cat > "$HOME/ex04-build-paths.sh" <<'EOF'
export SOURCE_TAG='20261101-ui'
export SOURCE_ARCHIVE="$HOME/upload/ex04-source-$SOURCE_TAG.tar.gz"
export PROJECT_ROOT="$HOME/ex04-work/ex04-source-$SOURCE_TAG"
export EZSDK='/home/user/ti-ezsdk_dm816x-evm_5_05_01_04'
export ARM_PREFIX='/home/user/arm-2014.05/bin/arm-none-linux-gnueabi-'
export ARM_SYSROOT="$EZSDK/linux-devkit/arm-none-linux-gnueabi"
export QT5_DIR="$EZSDK/filesystem/ezsdk-dm816x-evm-rootfs/home/qt_arm/qt5.9.7_install"
export QT5_PREFIX='/home/qt_arm/qt5.9.7_install'
export QMAKE_ARM="$QT5_PREFIX/bin/qmake"
export QT_BUILD="$PROJECT_ROOT/frontend_arm/build-qt597-arm"
export STAGE_ROOT="$PROJECT_ROOT/stage-arm-qt597"
export STAGE_APP="$STAGE_ROOT/ex04_syslink_dsp/release"
EOF
source "$HOME/ex04-build-paths.sh"

cd "$HOME/upload" || return 1
sed -i 's/\r$//' "ex04-source-$SOURCE_TAG.tar.gz.sha256" || return 1
sha256sum -c "ex04-source-$SOURCE_TAG.tar.gz.sha256" || return 1
if [ -e "$PROJECT_ROOT" ]; then
    printf '目录已存在：%s。保留目录，先按本节诊断检查；不要重复解压覆盖。\n' "$PROJECT_ROOT"
    return 1
fi
mkdir -p "$PROJECT_ROOT"
tar -xzf "$SOURCE_ARCHIVE" -C "$PROJECT_ROOT" || return 1
cd "$PROJECT_ROOT" || return 1
mkdir -p logs
sed -i 's/\r$//' run.sh
find frontend_arm -type f -name '*.sh' -exec sed -i 's/\r$//' {} +
ls shared/hrir_catalog.h dsp/dsp_spatial_coefficients.inc \
   frontend_arm/app/WaveformWidget.cpp frontend_arm/app/resources.qrc
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

通过标准：校验显示 `OK`；所列文件都存在。`tar` 解包后工程文件直接位于 `$PROJECT_ROOT`，不再多套一层 `ex04`。

从现在起，每开一个 Ubuntu 终端，先执行 `source "$HOME/ex04-build-paths.sh"`。本手册后面的 `PROJECT_ROOT` 都是 Ubuntu 路径，不是 Windows 的 `D:\...`。

### 第 3 节中断后如何检查

重新打开 Ubuntu Terminal，复制下列只读诊断。它不会退出终端，也不会删除或重新解压已有目录：

```bash
if [ -f "$HOME/ex04-build-paths.sh" ]; then
    source "$HOME/ex04-build-paths.sh"
    printf '用户=%s  HOME=%s\n' "$(whoami)" "$HOME"
    printf '源码包=%s\n工程目录=%s\n' "$SOURCE_ARCHIVE" "$PROJECT_ROOT"
    ls -lh "$SOURCE_ARCHIVE" "$SOURCE_ARCHIVE.sha256"
    (cd "$HOME/upload" && sed 's/\r$//' "ex04-source-$SOURCE_TAG.tar.gz.sha256" | sha256sum -c -)
    ls -ld "$PROJECT_ROOT"
    ls -l "$PROJECT_ROOT/shared/hrir_catalog.h" \
        "$PROJECT_ROOT/dsp/dsp_spatial_coefficients.inc" \
        "$PROJECT_ROOT/frontend_arm/app/WaveformWidget.cpp" \
        "$PROJECT_ROOT/frontend_arm/app/resources.qrc"
else
    printf '未找到路径配置：%s/ex04-build-paths.sh\n' "$HOME"
fi
```

- `No such file` 指向 upload 或压缩包：核对 XFTP 实际上传路径、登录用户与文件名。
- 文件名末尾出现 `$'\r'`：校验文件是 Windows 换行；用 `sed -i 's/\r$//' "$SOURCE_ARCHIVE.sha256"` 修正后重验，无需重传压缩包。
- 校验失败：核对压缩包和 `.sha256` 是否来自同一份下载，不跳过校验。
- 显示“目录已存在”：保留原目录。确认上次解压成功后可在新终端 source 路径并进入第 4 节；
  不确定是否完整时先保留诊断输出，未确认前不进入编译。

后续阅读请使用这份修订手册；现有源码包中的业务代码不受本次终端保护修订影响，无需为此重新传输源码。


## 4. Ubuntu：更新空间处理数据与检查源码

在未加载交叉编译环境的普通终端执行。需要本机 `gcc`、`make`、`python3`；缺少时安装 Ubuntu 的 `build-essential python3`。

### 4.1 本次只更新代码时

```bash
ex04_step() {
# [Ubuntu20；本机测试终端]
source "$HOME/ex04-build-paths.sh"
cd "$PROJECT_ROOT" || return 1
make hrir-check || return 1
make -C tests/spatial CC=gcc test test-catalog || return 1
python3 frontend_arm/tests/test_host_stop.py || return 1
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

HRIR 检查应显示 `710 directions`、`1420 WAVs`，空间算法及协议测试应全部通过。这些是 PC 参考/模拟测试，不会操作板端硬件。

### 4.2 以后增加或修改 HRIR 时

将配对 WAV 放入 Windows 源码的 `assets/hrir/mit_kemar`，重新打包上传，随后在 Ubuntu 执行：

```bash
ex04_step() {
# [Ubuntu20；HRIR 原始文件已经更新后]
source "$HOME/ex04-build-paths.sh"
cd "$PROJECT_ROOT" || return 1
make hrir || return 1
make hrir-check || return 1
make -C tests/spatial CC=gcc test test-catalog || return 1
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

命名如 `L60e250a.wav` / `R60e250a.wav`，要求单声道、44.1 kHz、PCM16、512 点，左右耳齐全。目录名沿用仓库中的 `MIT KERMER`，含空格，不能随意改名。

生成器会同时更新 `shared/hrir_catalog.h` 和 `dsp/dsp_spatial_coefficients.inc`。之后重编 host、DSP、Qt；只替换 WAV 不会改变已编译算法。当前生成器还要求保留 `40e045`、`60e250` 两组方向，供底层已有命令使用。

## 5. Ubuntu：编译 ARM host 与 DSP

### 5.1 核对工具链和 sysroot

使用**新的普通 Ubuntu 终端**，不 source Qt4 的 `environment-setup`。

```bash
ex04_step() {
# [Ubuntu20；host/DSP 构建终端]
source "$HOME/ex04-build-paths.sh"
cd "$PROJECT_ROOT" || return 1
"${ARM_PREFIX}gcc" --version
"${ARM_PREFIX}gcc" -print-sysroot
make .show DEPOT="$EZSDK" CGT_ARM_PREFIX="$ARM_PREFIX" ARM_SYSROOT="$ARM_SYSROOT"
ls "$ARM_SYSROOT/usr/include/alsa/asoundlib.h"
find "$ARM_SYSROOT" -name 'libasound.so*' -print
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

`ARM_SYSROOT` 是工程默认的 EZSDK 候选路径，需存在并包含 ARM ALSA 头文件/库；若你原先成功构建 host 使用另一套 sysroot，修改路径记录中的这一项，以原先匹配板端的配置为准。**Qt5 的编译则沿用其 mkspec/编译器配置，不把这个 host sysroot 强行加给 Qt。**

`make .show` 中 BIOS/IPC/SysLink/XDC/C674 的路径必须真实存在。默认版本由 `products.mak` 列出；若你的 SDK 组件目录不同，修改 Ubuntu 副本中的对应项，再继续。不要用 Ubuntu x86 的 ALSA 库补 ARM 链接。

### 5.2 构建与检查

```bash
ex04_step() {
# [Ubuntu20；同一 host/DSP 终端]
cd "$PROJECT_ROOT" || return 1
set -o pipefail
make -j2 DEPOT="$EZSDK" CGT_ARM_PREFIX="$ARM_PREFIX" ARM_SYSROOT="$ARM_SYSROOT" \
    2>&1 | tee logs/build-host-dsp.log || return 1
file host/bin/release/app_host dsp/bin/release/server_dsp.xe674
"${ARM_PREFIX}readelf" -l host/bin/release/app_host
"${ARM_PREFIX}readelf" -d host/bin/release/app_host
ls -lh dsp/bin/release/obj/server_dsp.xe674.map
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

通过标准：make 返回 0，debug/release 都生成；`app_host` 为 32 位 ARM，`server_dsp.xe674` 为 TI C674 DSP 产物。DSP 文件不是 ARM 程序。实际 map 中各段需成功放入工程的 16 MiB `DSP_PROG` 区，不能只看磁盘文件大小判断是否越界。

首次解包是干净源码，不需要先 clean。同目录更改 SDK、XDC 配置或工具链后，应先 `make clean` 再构建；该命令会清除本工程的 host/DSP 生成目录，先保留需要的日志和发布包。

## 6. Ubuntu：编译板端 Qt5.9.7

### 6.1 一次性建立 Qt5 路径映射

新开一个 Ubuntu 终端，不 source Qt4/EZSDK 的环境脚本。

```bash
ex04_step() {
# [Ubuntu20；ARM Qt5 构建终端]
source "$HOME/ex04-build-paths.sh"
test -f "$QT5_DIR/bin/qmake" || return 1
sudo mkdir -p /home/qt_arm || return 1
if [ ! -e "$QT5_PREFIX" ] && [ ! -L "$QT5_PREFIX" ]; then
    sudo ln -s "$QT5_DIR" "$QT5_PREFIX" || return 1
fi
if [ "$(readlink -f "$QT5_PREFIX")" != "$(readlink -f "$QT5_DIR")" ]; then
    printf 'Qt5 前缀已指向其他目录，请先核对：%s\n' "$QT5_PREFIX"
    return 1
fi
"$QMAKE_ARM" -v
"$QMAKE_ARM" -query
file -L "$QMAKE_ARM" "$QT5_PREFIX/bin/moc" "$QT5_PREFIX/lib/libQt5Core.so.5"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

预期 qmake 为 Qt5.9.7。qmake/moc 是运行在 Ubuntu 的 x86-64 工具，Qt5Core 是目标 ARM 库，这是正常的交叉编译组合。映射仅在 Ubuntu 创建；板端已有实际 Qt5 目录，不执行此符号链接操作。

### 6.2 构建 GUI、IPC peer 和显示探针

```bash
ex04_step() {
# [Ubuntu20；同一 ARM Qt5 终端]
export PATH="/home/user/arm-2014.05/bin:$PATH"
export QMAKESPEC=linux-TIarmv7-g++
cd "$PROJECT_ROOT" || return 1
set -o pipefail
BUILD_TARGET=arm BUILD_DIR="$QT_BUILD" \
    sh frontend_arm/build.sh "$QMAKE_ARM" \
    2>&1 | tee logs/build-qt597-arm.log || return 1

mkdir -p "$QT_BUILD/probe"
cd "$QT_BUILD/probe" || return 1
"$QMAKE_ARM" -nocache -spec linux-TIarmv7-g++ \
    "$PROJECT_ROOT/tests/qt_syslink_probe/qt_syslink_probe.pro" \
    CONFIG+=release CONFIG-=debug CONFIG-=debug_and_release || return 1
make -j2 || return 1

file "$QT_BUILD/bin/ex04_frontend_arm" "$QT_BUILD/bin/ex04_ipc_peer" ./qt_syslink_probe
"${ARM_PREFIX}readelf" -l "$QT_BUILD/bin/ex04_frontend_arm"
"${ARM_PREFIX}readelf" -d "$QT_BUILD/bin/ex04_frontend_arm"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

通过标准：三个程序都是 32 位 ARM；GUI 依赖 `libQt5Widgets` / `libQt5Gui` / `libQt5Core`，不能混入 Qt4 或 x86 库。构建脚本会拒绝非 ARM 产物；进一步用生成的 Makefile 核对编译器确为 `arm-none-linux-gnueabi-`。

这里的 qmake 目标配置为 ARMv7/Cortex-A8/NEON/softfp；不要切换成 `gnueabihf`。`QT_SYSROOT` 为空时沿用该 SDK/编译器的已有配置，不自行替换 sysroot。换 SDK 要用新的 `QT_BUILD` 目录。

`theme.qss` 和箭头资源已嵌入 GUI，不需要单独拷到板上；波形不依赖 Qt Charts、Qwt 或 OpenGL。

### 6.3 可选：先在虚拟机桌面预览界面

这一节使用**另一个未加载 ARM 环境的普通终端**。先检查 `/usr/bin/qmake -v` 为桌面 Qt5，配套 Qt 库为 x86/x86-64。缺依赖时安装 `qtbase5-dev qt5-qmake qtbase5-dev-tools build-essential fonts-wqy-microhei`。

```bash
ex04_step() {
# [Ubuntu20；桌面 Qt5 终端]
source "$HOME/ex04-build-paths.sh"
cd "$PROJECT_ROOT" || return 1
BUILD_TARGET=desktop BUILD_DIR="$PROJECT_ROOT/frontend_arm/build-desktop" \
    sh frontend_arm/build.sh /usr/bin/qmake || return 1
./frontend_arm/build-desktop/bin/ex04_frontend_arm --windowed
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

程序默认进入演示环境，可先验收分页、参数与四轨波形。这个目录的程序仅供 Ubuntu 使用；后面的打包始终指定 `$QT_BUILD`，避免装错版本。自动化测试命令见 [Qt README](../frontend_arm/README.md#波形与-gui-自动验收桌面-qt5)。

## 7. Ubuntu：合成 ARM 发布目录和部署包

新开普通终端，读取同一份路径记录。host/DSP 与 ARM Qt 构建都成功后再执行。

```bash
ex04_step() {
# [Ubuntu20；发布打包终端]
source "$HOME/ex04-build-paths.sh"
cd "$PROJECT_ROOT" || return 1
make install EXEC_DIR="$STAGE_ROOT" DEPOT="$EZSDK" \
    CGT_ARM_PREFIX="$ARM_PREFIX" ARM_SYSROOT="$ARM_SYSROOT" || return 1
BUILD_DIR="$QT_BUILD" sh frontend_arm/install.sh "$STAGE_APP" || return 1
install -m 755 "$QT_BUILD/probe/qt_syslink_probe" "$STAGE_APP/" || return 1
install -m 644 tests/spatial/input.wav "$STAGE_APP/input.wav" || return 1
chmod 755 "$STAGE_APP/app_host" "$STAGE_APP/slaveloader" \
    "$STAGE_APP/run.sh" "$STAGE_APP/run-qt5.sh" || return 1

printf 'SOURCE_TAG=%s\nQT_TARGET=5.9.7 ARM\nBUILT_AT=%s\n' \
    "$SOURCE_TAG" "$(date -Iseconds)" > "$STAGE_APP/BUILD_INFO.txt" || return 1
file "$STAGE_APP/app_host" "$STAGE_APP/ex04_frontend_arm" \
    "$STAGE_APP/ex04_ipc_peer" "$STAGE_APP/slaveloader"
ls -lh "$STAGE_APP"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

注意：根目录 `make install EXEC_DIR=...` 会自动追加 `ex04_syslink_dsp/release`；Qt 的 `install.sh` 接收的是**最终 release 目录**。不要给两者传同一个末级目录。

最终 release 至少包含：

```text
app_host                  ARM 音频主程序
server_dsp.xe674           DSP 空间处理固件
slaveloader                与 SDK 匹配的 DSP 加载器
run.sh                    命令行运行入口
ex04_frontend_arm         ARM Qt GUI
ex04_ipc_peer              Qt IPC 验证的独立 C 程序
qt_syslink_probe           显示/VNC 探针
run-qt5.sh / run-qt4.sh    运行脚本；本次使用 run-qt5.sh
input.wav                 验证用 44.1 kHz / stereo / PCM16 输入
BUILD_INFO.txt            本次源码标签和构建时间
```

生成校验清单和压缩包：

```bash
ex04_step() {
# [Ubuntu20；同一发布终端]
source "$HOME/ex04-build-paths.sh" || return 1
mkdir -p "$PROJECT_ROOT/delivery" || return 1
(
    set -o pipefail
    cd "$STAGE_ROOT" || return 1
    find ex04_syslink_dsp/release -type f -print0 \
        | sort -z | xargs -0 sha256sum > SHA256SUMS || return 1
    sha256sum -c SHA256SUMS || return 1
) || return 1
DEPLOY_ARCHIVE="$PROJECT_ROOT/delivery/ex04-dm8168-qt597-$SOURCE_TAG.tar.gz"
tar -czf "$DEPLOY_ARCHIVE" -C "$STAGE_ROOT" \
    ex04_syslink_dsp/release SHA256SUMS || return 1
sha256sum "$DEPLOY_ARCHIVE" || return 1
tar -tzf "$DEPLOY_ARCHIVE" || return 1
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

本包只装业务文件，沿用板端已有 Qt5.9.7 运行库。不要把 Ubuntu `/usr/lib/x86_64-linux-gnu`、SDK 的宿主机 qmake/moc 或桌面库装到板端。HRIR 已编入 DSP，运行时不需要再复制原始 HRIR 文件夹。

## 8. 虚拟机 → 板端：上传、校验、备份更新

### 8.1 默认采用 HTTP/wget

当前只确认板端有 Telnet，NFS 映射尚未确认，因此采用 HTTP/wget。板端已回传 `-sh: command: not found`；此前使用 `command -v` 得到的 `MISSING_WGET` / `MISSING_MD5SUM` 不能证明这些工具不存在。BusyBox 的 `sha256sum: applet not found` 则说明该 applet 确实未内置，独立工具仍需实际检测。

本节使用 [Ubuntu 校验准备脚本](../tools/deploy/prepare-checksums.sh) 和 [板端部署脚本](../tools/deploy/deploy-board.sh)，通过无副作用的空文件哈希实际选择 SHA256 或 MD5。板端不再使用 `command`、`type`、`which`。脚本在子进程运行，失败不会关闭交互终端；现有源码和 ARM 发布归档沿用原件。

先把 Windows 的 [部署辅助包](../delivery/ex04-board-deploy-20261101.tar.gz) 经 XFTP 以二进制方式上传到 Ubuntu：

```text
/home/user/upload/ex04-board-deploy-20261101.tar.gz
```

在 Ubuntu 普通 Bash 执行，解压辅助脚本并生成归档 `.sha256`、`.md5` 和文件 `MD5SUMS`。准备脚本会先确认归档与暂存目录的 SHA256 清单相同且暂存文件校验通过：

```bash
ex04_step() {
# [Ubuntu20；校验准备终端]
source "$HOME/ex04-build-paths.sh" || return 1
tar -xzf "$HOME/upload/ex04-board-deploy-20261101.tar.gz" \
    -C "$PROJECT_ROOT/delivery" || return 1
bash "$PROJECT_ROOT/delivery/board-deploy/prepare-checksums.sh" || return 1
}
if ex04_step; then
    printf '校验文件已准备，确认输出 CHECKSUMS_READY。\n'
else
    printf '准备失败，请保留上方输出，暂不部署。\n' >&2
fi
unset -f ex04_step
```

保持已有的 Ubuntu 8000 文件服务运行，其目录必须是 `$PROJECT_ROOT/delivery`；尚未启动时，在另一个终端执行：

```bash
ex04_step() {
# [Ubuntu20；文件服务终端]
source "$HOME/ex04-build-paths.sh" || return 1
hostname -I
python3 -m http.server 8000 --bind 0.0.0.0 \
    --directory "$PROJECT_ROOT/delivery"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

记下板端可访问的 **Ubuntu IPv4 地址**，不能把板端 `192.168.1.21` 填成 Ubuntu 地址。
本次用户回传的同网段地址是 `192.168.1.41`。若 8000 端口仍由之前的探针文件服务占用，在原服务终端按 Ctrl+C 后再启动本段服务。

先在板端 GUI 中停止正在进行的任务，等待清理结束，再点“退出”；显示探针在窗口内按 Escape 退出，或在启动探针的前台板端终端按 Ctrl+C。确认没有另一条 CLI 音频任务运行。然后在板端 Telnet/串口 Linux shell 执行以下短命令，下载脚本并作为子进程运行。地址或标签改变时同步修改两处参数：

```sh
ex04_step() {
# [DM8168 Linux]
wget -O /tmp/ex04-deploy-board.sh \
    http://192.168.1.41:8000/board-deploy/deploy-board.sh || return 1
sh /tmp/ex04-deploy-board.sh 192.168.1.41 20261101-ui || return 1
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

通过标准：输出 `HASH_TOOL=...`，整包及解压/安装后文件校验全部 `OK`，最后出现 `DEPLOY_OK=/home/root/ex04_syslink_dsp/release`。若旧 release 存在，会输出 `BACKUP_DIR=...`；原有用户 WAV 保留。脚本只安装文件，不启动 GUI 或 DSP。

首次成套升级时，`BUILD_INFO.txt` 标签应与本次源码一致；仅 Qt 更新时，保留其中的处理程序旧标签，部署后另行核对 `UI_UPDATE_INFO.txt` 的 `UI_SOURCE_TAG`。包或解压文件校验失败时不会覆盖旧 release；复制或安装后校验失败时，保留旧版本备份并依据最后的 `STEP=` 输出处理，未成功前不启动项目。

若独立命令和 BusyBox 的 SHA256 / MD5 均不能运行，脚本会明确报错并停止，届时回传 `HASH_TOOL`、`STEP` 和错误输出，不能跳过校验。完成部署后，Ubuntu 服务终端按 Ctrl+C 结束服务。辅助脚本的本机隔离验证范围见 [脚本说明](../tools/deploy/README.md)，与真实板端部署结果分开记录。

### 8.2 只有 NFS 映射确认后才用直接复制

先用板端 `/proc/cmdline`、挂载信息和 Ubuntu `sudo exportfs -v` 确认根文件系统映射，不能仅因两端目录相似就认定是同一目录。已确认时，可从 Ubuntu 将 `$STAGE_APP/.` 合并到：

```text
$NFS_ROOT/home/root/ex04_syslink_dsp/release/
```

复制前同样正常关闭 GUI/任务并备份旧 release。完整 NFS 命令见 [环境配置报告第 8 节](arm-qt-environment-setup-report.md#8-ubuntu20先暂存再部署到板端可见目录)。

## 9. 板端：先验证 Qt5 VNC，再运行新版 GUI

### 9.1 每个新板端会话都要设置运行环境

```sh
ex04_step() {
# [DM8168 Linux]
APPDIR=/home/root/ex04_syslink_dsp/release
QT5_DIR=/home/qt_arm/qt5.9.7_install
export LD_LIBRARY_PATH="$QT5_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$QT5_DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$QT5_DIR/plugins/platforms"
cd "$APPDIR" || return 1
ls -l "$QT5_DIR/lib/libQt5Core.so.5" "$QT5_DIR/plugins/platforms/libqvnc.so"
netstat -lnt
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

确认 5900 未被已有进程占用。板端没有 `ldd`，启动失败时查看插件日志，并对照第 6 节 readelf 给出的 ELF 解释器和依赖名称。

### 9.2 尚未通过 VNC 时先运行探针

```sh
ex04_step() {
# [DM8168 Linux；当前终端前台运行]
QT_DEBUG_PLUGINS=1 ./qt_syslink_probe \
    -platform 'vnc:size=800x480:port=5900' \
    > /tmp/ex04-qt5-probe.log 2>&1
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

前台持续运行是正常现象。在另一个板端会话查看 `tail -n 80 /tmp/ex04-qt5-probe.log` 和 `netstat -lnt`。Windows VNC 客户端连接 **192.168.1.21，端口 5900**；能看到时间/色块更新、点击计数增加后，在画面中按 Escape 退出探针，释放 5900。

具体排错见 [Qt5 VNC 探针说明](qt5-vnc-probe-runbook.md)。插件文件存在与探针真正可用是两个验收项。

### 9.3 启动正式 GUI

探针退出后，在板端会话整段执行以下命令。新版 GUI 默认使用演示环境，启动本身不加载 DSP；本节先验收界面与波形。命令会检查 5900 占用，并在 GUI 退出时打印退出码和日志：

```sh
ex04_step() {
# [DM8168 Linux；前台运行]
cd /home/root/ex04_syslink_dsp/release || return 1
if netstat -lnt | grep ':5900[[:space:]]'; then
    printf 'PORT_BUSY=5900\n' >&2
    return 1
fi
QT5_DIR=/home/qt_arm/qt5.9.7_install
export LD_LIBRARY_PATH="$QT5_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$QT5_DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$QT5_DIR/plugins/platforms"
cat BUILD_INFO.txt || return 1
QT_DEBUG_PLUGINS=1 ./ex04_frontend_arm \
    -platform 'vnc:size=800x480:port=5900' > /tmp/ex04-gui.log 2>&1
EX04_GUI_RC=$?
printf 'GUI_EXIT=%s\n' "$EX04_GUI_RC"
tail -n 60 /tmp/ex04-gui.log
return "$EX04_GUI_RC"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

运行期间终端暂不返回提示符是正常现象。在板端用 `netstat -lnt` 查看监听状态。Qt5.9.7 与 TigerVNC 1.16.2 直接连接曾黑屏及报 `Cannot handle 0 bpp client`，固定 256 色也未能稳定解决。当前使用本机兼容转发显式初始化像素格式，用户已确认全彩中文、波形和断开重连正常。Windows 双击 `tools/deploy/start-vnc.cmd`，或在 PowerShell 执行：

```powershell
& 'D:\develop\vscode\projects\ex04\tools\deploy\start-vnc.cmd'
```

脚本启动本机 Python 3 后台转发，再打开查看器连接 `127.0.0.1::5901`，转发到板端 `192.168.1.21:5900`。后续重复启动会复用转发。转发已运行时，也可打开 [连接配置](../delivery/ex04-vnc-via-bridge.tigervnc)。详细流程和日志位置见 [兼容转发说明](../tools/deploy/VNC_BRIDGE.md)。不再用纯 TCP 连接测试这个旧 VNC 插件。然后按以下顺序操作：

1. 默认“演示”环境：切换波形、参数、诊断、设置，观察动态波形。
2. 波形页导入 `input.wav`，放大、拖动、点击查看采样值。
3. 诊断页分别验证信号量、共享内存，等到 10 条消息完成；停止后重试。
4. 设置页工作目录保持 `/home/root/ex04_syslink_dsp/release`。
5. 完成下一节的空间链路验证后，再切换“板端”环境运行真实处理。

若中文显示方块，先完成下一节的字库安装，再验收页面文字。

目前真实实时 PCM 还没有接入 GUI，板端音频运行时波形为空不等于处理失败。文件处理保存成功后，会自动加载输出 WAV 显示；演示波形不会冒充板端数据。

有 HDMI 时，沿用已验证的 framebuffer/输入环境改用 `QT_QPA_PLATFORM=linuxfb` 启动。显示固件与 DSP 共存需按 [共存验收方案](qt-syslink-coexistence-test.md) 核对；不通过停止显示固件来“修好”音频。

### 9.4 安装中文字库并重启 GUI

无需重编译。使用本机 [中文字库包](../delivery/ex04-cjk-fonts-20261101.tar.gz)，约 2.3 MB，包含文泉驿微米黑、许可证、字体配置和 [启动脚本](../tools/deploy/run-gui-vnc.sh)。SHA256 为 `b72df21043c4c17156198e7dba80cad95fd8403e84e5a4da5e0eaeb267f3d38d`。

用 XFTP 将该包上传到 Ubuntu 正在通过 HTTP 8000 提供下载的目录：

```text
/home/user/ex04-work/ex04-source-20261101-ui/delivery/
```

若 HTTP 服务已经关闭，在 Ubuntu 新终端执行 `source "$HOME/ex04-build-paths.sh"`、`cd "$PROJECT_ROOT/delivery"`，再执行 `python3 -m http.server 8000 --bind 0.0.0.0`，保持该终端打开。

先在 VNC 内正常停止任务，再点 GUI 右上角“退出”。中文未显示时，该按钮是顶栏最右侧的按钮。关闭 TigerVNC 窗口只断开查看器，不会退出 GUI。

板端串口或 Telnet 执行：

```sh
ex04_fonts() {
    wget -O /tmp/ex04-cjk-fonts.tar.gz \
        http://192.168.1.41:8000/ex04-cjk-fonts-20261101.tar.gz || return 1
    tar -xzf /tmp/ex04-cjk-fonts.tar.gz -C /home/root || return 1
    (
        cd /home/root/ex04-fonts || return 1
        if sha256sum /dev/null >/dev/null 2>&1; then
            sha256sum -c SHA256SUMS
        elif busybox sha256sum /dev/null >/dev/null 2>&1; then
            busybox sha256sum -c SHA256SUMS
        elif md5sum /dev/null >/dev/null 2>&1; then
            md5sum -c MD5SUMS
        elif busybox md5sum /dev/null >/dev/null 2>&1; then
            busybox md5sum -c MD5SUMS
        else
            printf 'ERROR: no SHA256 or MD5 tool\n' >&2
            return 1
        fi
    ) || return 1
    sh /home/root/ex04-fonts/run-gui-vnc.sh \
        > /tmp/ex04-gui.log 2>&1 < /dev/null &
    EX04_GUI_PID=$!
    printf 'GUI_PID=%s\n' "$EX04_GUI_PID"
}
if ex04_fonts; then
    printf '字库已安装，GUI 已请求启动，请通过本机兼容转发重连。\n'
else
    printf '安装失败，请保留错误输出。\n' >&2
fi
unset -f ex04_fonts
```

`GUI_PID` 表示后台启动已提交，不能单独证明应用启动成功。启动脚本检查 5900 占用，设置 `QT_QPA_FONTDIR` 和 `FONTCONFIG_FILE` 后启动 GUI；若仍有旧 GUI，会报 `PORT_BUSY=5900`。按 9.3 的 Windows 启动脚本重连，预期中文标签正常、蓝白界面和波形仍可见。异常时回传 `tail -n 80 /tmp/ex04-gui.log`。

以后启动 GUI 只需执行 `sh /home/root/ex04-fonts/run-gui-vnc.sh > /tmp/ex04-gui.log 2>&1 < /dev/null &`，无需重复下载。本次安装后已直接取得板端 32 位及 256 色画面，中文绘制正常；用户查看器仍黑屏时，应继续排查查看器连接状态，不能把它等同于字库安装失败，详见 [实际帧与验证记录](evidence/arm-build-2026-11-01/README.md)。

## 10. 板端：验证新版空间处理程序

先正常退出 GUI；CLI 和 GUI 不同时运行音频任务。以下先做不接触音频设备/DSP 的参数检查：

```sh
ex04_step() {
# [DM8168 Linux]
cd /home/root/ex04_syslink_dsp/release || return 1
sh ./run.sh directions > /tmp/ex04-directions.txt
head -n 8 /tmp/ex04-directions.txt
sh ./run.sh file --input input.wav --output out-single.wav \
    --algo spatial --elevation 0 --azimuth 90 --check
sh ./run.sh file --input input.wav --output out-dual.wav \
    --algo dual --a-elevation 0 --a-azimuth 270 \
    --b-elevation 0 --b-azimuth 90 --check
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

在现有 SysLink/内存及 DSP 固件环境已经核对通过的前提下，再运行实际处理：

```sh
ex04_step() {
# [DM8168 Linux；已有 SysLink 基线与共存配置可用]
sh ./run.sh smoke --algo passthrough > /tmp/ex04-smoke.log 2>&1
cat /tmp/ex04-smoke.log
sh ./run.sh file --input input.wav --output out-single.wav \
    --algo spatial --elevation 0 --azimuth 90 > /tmp/ex04-spatial.log 2>&1
cat /tmp/ex04-spatial.log
ls -lh out-single.wav
sh ./run.sh file --input input.wav --output out-dual.wav \
    --algo dual --a-elevation 0 --a-azimuth 270 \
    --b-elevation 0 --b-azimuth 90 > /tmp/ex04-dual.log 2>&1
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

成功判据：返回码 0、日志无初始化/处理失败、DSP 正常 shutdown，输出 WAV 生成。文件输出模式完成后 host 可能继续通过 `aplay` 播放结果，等待任务自然退出后再开启下一项。

重新打开 GUI 并切换“板端”环境：参数选文件处理，输入 `input.wav`，勾选保存，选择新的输出名，算法选单声源或双声源并设置方向，再开始。完成后检查输出波形与诊断日志。波形页的“对比 WAV”也可手动打开刚才的 CLI 输出。

录音和实时处理仍使用新版参数，例如：

```sh
ex04_step() {
# [DM8168 Linux；分别运行，不与 GUI 任务并发]
sh ./run.sh record --output record.wav --seconds 5
sh ./run.sh audio --seconds 5 --algo spatial --elevation 0 --azimuth 90
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

## 11. 以后每次更新怎样选择步骤

| 修改内容 | 要重新做什么 |
|---|---|
| 本次首次升级；参数协议、方向目录或 HRIR 改动 | 源码包 → 第 3～10 节；host / DSP / Qt 成套更新 |
| 仅 Qt 布局、样式、按钮逻辑 | 源码包 → 第 3 节 → 第 6 节重编 Qt；先复制上一份成功发布目录到新暂存目录，再用 Qt install.sh 覆盖 GUI/peer 和脚本，重新打包校验 |
| 空间算法代码改动 | 重跑第 4 节测试，重新构建 host/DSP；若共用头文件/方向目录改动，Qt 一起重编；部署时保留配套版本 |
| 更换编译器、Qt SDK 或 sysroot | 新源码/构建目录重新构建，并重新验证板端依赖、ABI 与显示，不复用旧目标文件 |

纯 Qt 更新示例：`PREVIOUS_RELEASE` 必须指向上一份**已通过板端验证**的完整 release 备份或暂存目录。

```bash
ex04_step() {
# [Ubuntu20；Qt 已按第 6 节重新构建]
source "$HOME/ex04-build-paths.sh"
PREVIOUS_RELEASE='/替换为上一份成功版本的完整release目录'
test -f "$PREVIOUS_RELEASE/app_host" || return 1
test -f "$PREVIOUS_RELEASE/server_dsp.xe674" || return 1
mkdir -p "$STAGE_APP"
cp -a "$PREVIOUS_RELEASE/." "$STAGE_APP/" || return 1
BUILD_DIR="$QT_BUILD" sh "$PROJECT_ROOT/frontend_arm/install.sh" "$STAGE_APP" || return 1
printf 'UI_SOURCE_TAG=%s\nUI_UPDATED_AT=%s\n' \
    "$SOURCE_TAG" "$(date -Iseconds)" > "$STAGE_APP/UI_UPDATE_INFO.txt"
}
if ex04_step; then
    printf '本段执行结束，请按通过标准核对。\n'
else
    printf '本段失败，保留上方输出；不要继续下一段。\n' >&2
fi
unset -f ex04_step
```

然后执行第 7 节的**生成校验清单和压缩包**部分，不再执行该节开头的 host/DSP `make install`。保留原 `BUILD_INFO.txt` 记录处理程序版本，新增 `UI_UPDATE_INFO.txt` 记录 GUI 版本，避免把旧处理程序误标成本次重新构建。

如果升级失败，先正常结束业务，再从 `/home/root/ex04-release-backup-标签` 恢复整套 release；不要只把新旧 host/DSP 混着换。保留本次 `/tmp/ex04-*.log`。有 DSP 清理异常时先核查板端状态，不能靠反复点击开始恢复。

## 12. 常见问题速查

| 现象 | 检查点 |
|---|---|
| `^M`、`\r`、坏解释器 | 第 3 节对 Linux shell 脚本去 CRLF；XFTP 传压缩包用二进制 |
| Qt 缺 shared/hrir_catalog.h | 传完整源码包，包含 shared；不要只传 frontend_arm |
| qmake 找不到 include/modules | 第 6.1 节 SDK 前缀映射；保留原目录，不强制覆盖 |
| `Expected 32-bit ARM ELF` | 用错桌面 qmake/mkspec，或混用 build 目录 |
| `cannot find -lasound` | host 的 ARM sysroot 与 ALSA 开发库；不是安装 x86 ALSA 即可解决 |
| `VFP` / hard-float / softfp 冲突 | 工具链和 Qt mkspec 是否与板端同套；当前不是 gnueabihf |
| 文件存在却报 `not found` | readelf 的 ELF 解释器在板上是否存在；不是简单 chmod 问题 |
| `GLIBC` / `GLIBCXX` 版本不匹配 | 重新选兼容板端的工具链/sysroot；不替换系统 libc 试错 |
| VNC 无法加载 | 查看完整 QT_DEBUG_PLUGINS 日志，确认 Qt5 库、插件与依赖 |
| GUI 等不到 host 就绪 | 确认本次 app_host 已部署，日志有 `main: control ready` |
| 角度被拒绝 | 对照 directions；只支持目录中的组合，不插值或自动取邻近方向 |
| 只传 HRIR WAV 后效果未变 | 系数编入 DSP；执行 make hrir 并重新构建/部署配套程序 |

遇到构建错误时，回传**完整命令、第一处错误、build-host-dsp.log 或 build-qt597-arm.log**。遇到运行错误时，回传 BUILD_INFO、ELF 依赖检查结果和 `/tmp/ex04-*.log`，便于确认具体版本与失败阶段。
