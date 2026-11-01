#!/bin/sh
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${BUILD_DIR:-"$SOURCE_DIR/build"}
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /mounted/rootfs/home/root/ex04_syslink_dsp/release" >&2
    exit 2
fi
DESTINATION=$1
for binary in ex04_frontend_arm ex04_ipc_peer; do
    test -f "$BUILD_DIR/bin/$binary"
done
mkdir -p "$DESTINATION"
install -m 755 "$BUILD_DIR/bin/ex04_frontend_arm" "$DESTINATION/"
install -m 755 "$BUILD_DIR/bin/ex04_ipc_peer" "$DESTINATION/"
install -m 755 "$SOURCE_DIR/run-qt4.sh" "$DESTINATION/"
install -m 755 "$SOURCE_DIR/run-qt5.sh" "$DESTINATION/"
printf 'Installed Qt application and IPC peer to %s\n' "$DESTINATION"
