#!/bin/sh
set -eu

APPDIR=/home/root/ex04_syslink_dsp/release
QT5_DIR=/home/qt_arm/qt5.9.7_install
EX04_FONT_DIR=/home/root/ex04-fonts

cd "$APPDIR" || exit 1
if netstat -lnt | grep ':5900[[:space:]]'; then
    printf 'ERROR: PORT_BUSY=5900\n' >&2
    exit 1
fi
if [ ! -r "$EX04_FONT_DIR/wqy-microhei.ttc" ]; then
    printf 'ERROR: missing %s/wqy-microhei.ttc\n' "$EX04_FONT_DIR" >&2
    exit 1
fi
mkdir -p /tmp/ex04-fontconfig-cache || exit 1
export LD_LIBRARY_PATH="$QT5_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$QT5_DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$QT5_DIR/plugins/platforms"
export QT_QPA_FONTDIR="$EX04_FONT_DIR"
export FONTCONFIG_FILE="$EX04_FONT_DIR/fonts.conf"
exec ./ex04_frontend_arm -platform 'vnc:size=800x480:port=5900' "$@"
