#!/bin/sh
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_TARGET=${BUILD_TARGET:-arm}
QMAKE_TOOL=${1:-${QMAKE_ARM:-}}
BUILD_DIR=${BUILD_DIR:-"$SOURCE_DIR/build"}
MAKE_TOOL=${MAKE:-make}

if [ -z "$QMAKE_TOOL" ]; then
    echo "Usage: $0 /path/to/ARM-Qt/bin/qmake" >&2
    echo "Desktop test: BUILD_TARGET=desktop $0 /usr/bin/qmake" >&2
    exit 2
fi
case "$BUILD_TARGET" in arm|desktop) ;; *) echo "BUILD_TARGET must be arm or desktop" >&2; exit 2 ;; esac
command -v "$QMAKE_TOOL" >/dev/null
command -v "$MAKE_TOOL" >/dev/null
command -v file >/dev/null
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
"$QMAKE_TOOL" -query
"$QMAKE_TOOL" "$SOURCE_DIR/frontend_arm.pro"
"$MAKE_TOOL" -j"${JOBS:-2}"
for binary in bin/ex04_frontend_arm bin/ex04_ipc_peer; do
    description=$(file -b "$binary")
    printf '%s: %s\n' "$binary" "$description"
    if [ "$BUILD_TARGET" = arm ]; then
        case "$description" in
            *ELF*ARM*) ;;
            *) echo "Expected 32-bit ARM ELF. Check the selected ARM Qt qmake/mkspec." >&2; exit 1 ;;
        esac
    fi
done
