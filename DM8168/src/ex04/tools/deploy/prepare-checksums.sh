#!/bin/bash
# Run on Ubuntu20: bash prepare-checksums.sh [paths-file]
set -euo pipefail

source "${1:-$HOME/ex04-build-paths.sh}"
ARCHIVE_NAME="ex04-dm8168-qt597-$SOURCE_TAG.tar.gz"
DELIVERY_DIR="$PROJECT_ROOT/delivery"
ARCHIVE="$DELIVERY_DIR/$ARCHIVE_NAME"

cd "$STAGE_ROOT"
printf 'STEP=verify-stage\n'
tar -xOf "$ARCHIVE" SHA256SUMS | cmp - SHA256SUMS
sha256sum -c SHA256SUMS

printf 'STEP=prepare-checksums\n'
find ex04_syslink_dsp/release -type f -print0 \
    | sort -z | xargs -0 md5sum > "$DELIVERY_DIR/MD5SUMS"
cd "$DELIVERY_DIR"
sha256sum "$ARCHIVE_NAME" > "$ARCHIVE_NAME.sha256"
md5sum "$ARCHIVE_NAME" > "$ARCHIVE_NAME.md5"
cat "$ARCHIVE_NAME.sha256" "$ARCHIVE_NAME.md5"
printf 'CHECKSUMS_READY=%s\n' "$DELIVERY_DIR"
