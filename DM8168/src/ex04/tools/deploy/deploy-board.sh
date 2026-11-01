#!/bin/sh
# Run on the board: sh deploy-board.sh [ubuntu-ip] [source-tag]
# EX04_DEPLOY_ROOT may select an isolated test/deployment root.
set -eu

UBUNTU_IP=${1:-192.168.1.41}
SOURCE_TAG=${2:-20261101-ui}
DEPLOY_ROOT=${EX04_DEPLOY_ROOT:-/home/root}
ARCHIVE_NAME="ex04-dm8168-qt597-$SOURCE_TAG.tar.gz"
BASE_URL="http://$UBUNTU_IP:8000"
UPDATE_TAG=$(date +%Y%m%d-%H%M%S)-$$
INCOMING="$DEPLOY_ROOT/ex04-incoming/$UPDATE_TAG"
APPDIR="$DEPLOY_ROOT/ex04_syslink_dsp/release"
BACKUP_DIR="$DEPLOY_ROOT/ex04-release-backup-$UPDATE_TAG"

# Execute a harmless hash operation; do not depend on shell lookup builtins.
if sha256sum /dev/null >/dev/null 2>&1; then
    HASH_TOOL=sha256sum
    HASH_SUFFIX=sha256
    FILE_MANIFEST=SHA256SUMS
elif busybox sha256sum /dev/null >/dev/null 2>&1; then
    HASH_TOOL=busybox-sha256sum
    HASH_SUFFIX=sha256
    FILE_MANIFEST=SHA256SUMS
elif md5sum /dev/null >/dev/null 2>&1; then
    HASH_TOOL=md5sum
    HASH_SUFFIX=md5
    FILE_MANIFEST=MD5SUMS
elif busybox md5sum /dev/null >/dev/null 2>&1; then
    HASH_TOOL=busybox-md5sum
    HASH_SUFFIX=md5
    FILE_MANIFEST=MD5SUMS
else
    printf 'ERROR: no working SHA256 or MD5 tool; no files installed.\n' >&2
    exit 1
fi

check_manifest() {
    case "$HASH_TOOL" in
        sha256sum) sha256sum -c "$1" ;;
        busybox-sha256sum) busybox sha256sum -c "$1" ;;
        md5sum) md5sum -c "$1" ;;
        busybox-md5sum) busybox md5sum -c "$1" ;;
    esac
}

printf 'HASH_TOOL=%s\nSTEP=check-processes\n' "$HASH_TOOL"
if ps | grep -E '[e]x04_frontend|[e]x04_ipc|[a]pp_host|[q]t_syslink|[s]laveloader'; then
    printf 'ERROR: stop project processes before deployment.\n' >&2
    exit 1
fi

mkdir -p "$DEPLOY_ROOT/ex04-incoming" || exit 1
mkdir "$INCOMING" || exit 1
cd "$INCOMING" || exit 1
df -h "$DEPLOY_ROOT"
printf 'STEP=download\n'
wget -O "$ARCHIVE_NAME" "$BASE_URL/$ARCHIVE_NAME" || exit 1
wget -O "$ARCHIVE_NAME.$HASH_SUFFIX" \
    "$BASE_URL/$ARCHIVE_NAME.$HASH_SUFFIX" || exit 1

printf 'STEP=verify-archive\n'
check_manifest "$ARCHIVE_NAME.$HASH_SUFFIX" || exit 1
tar -xzf "$ARCHIVE_NAME" || exit 1
if [ "$FILE_MANIFEST" = MD5SUMS ]; then
    wget -O MD5SUMS "$BASE_URL/MD5SUMS" || exit 1
fi
printf 'STEP=verify-extracted-files\n'
check_manifest "$FILE_MANIFEST" || exit 1

printf 'STEP=backup\n'
if [ -d "$APPDIR" ]; then
    cp -a "$APPDIR" "$BACKUP_DIR" || exit 1
    printf 'BACKUP_DIR=%s\n' "$BACKUP_DIR"
fi

printf 'STEP=install\n'
mkdir -p "$APPDIR" || exit 1
cp -a "$INCOMING/ex04_syslink_dsp/release/." "$APPDIR/" || exit 1
cd "$DEPLOY_ROOT" || exit 1
printf 'STEP=verify-installed-files\n'
check_manifest "$INCOMING/$FILE_MANIFEST" || exit 1
cd "$APPDIR" || exit 1
chmod 755 app_host slaveloader run.sh ex04_frontend_arm \
    ex04_ipc_peer qt_syslink_probe run-qt5.sh || exit 1
cat BUILD_INFO.txt || exit 1
printf 'DEPLOY_OK=%s\n' "$APPDIR"
