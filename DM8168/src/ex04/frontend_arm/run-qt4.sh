#!/bin/sh
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$APP_DIR"
exec ./ex04_frontend_arm -qws "$@"
