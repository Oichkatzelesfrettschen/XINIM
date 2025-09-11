#!/usr/bin/env bash
# Create a stub XINIM image using raw blobs and no patching.
# Produces minimal placeholders suitable for QEMU plumbing validation.

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build"
OUT_IMG="$BUILD_DIR/xinim-stub.img"
BUILDER="$BUILD_DIR/tools/minix_tool_build"

mkdir -p "$BUILD_DIR/stubs" "$BUILD_DIR/tools"

# Ensure builder exists
if [[ ! -x "$BUILDER" ]]; then
  echo "error: image builder not found at $BUILDER" >&2
  echo "hint: run 'ninja -C build minix_tool_build' first" >&2
  exit 2
fi

# Create a 512-byte bootblock
BOOTBLOCK="$BUILD_DIR/stubs/bootblok"
dd if=/dev/zero of="$BOOTBLOCK" bs=512 count=1 status=none

# Create small raw program blobs (1 KiB each)
for name in kernel mm fs init fsck; do
  dd if=/dev/zero of="$BUILD_DIR/stubs/$name" bs=1024 count=1 status=none
done

"$BUILDER" --raw --no-patch \
  "$BOOTBLOCK" \
  "$BUILD_DIR/stubs/kernel" \
  "$BUILD_DIR/stubs/mm" \
  "$BUILD_DIR/stubs/fs" \
  "$BUILD_DIR/stubs/init" \
  "$BUILD_DIR/stubs/fsck" \
  "$OUT_IMG"

echo "Stub image created: $OUT_IMG"

