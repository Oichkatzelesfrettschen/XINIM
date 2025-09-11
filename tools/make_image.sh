#!/usr/bin/env bash
# Compose a bootable MINIX image using the modern builder.
# Usage:
#   tools/make_image.sh <builder> <bootblock> <kernel> <mm> <fs> <init> <fsck> <output.img>
# Notes:
#   - <bootblock> must be <= 512 bytes. It will be padded/truncated to exactly 512 bytes.
#   - The builder is typically build/tools/minix_tool_build (CMake target: minix_tool_build).

set -euo pipefail

if [[ $# -ne 8 ]]; then
  echo "Usage: $0 <builder> <bootblock> <kernel> <mm> <fs> <init> <fsck> <output.img>" >&2
  exit 2
fi

BUILDER="$1"; shift
BOOTBLOCK="$1"; shift
KERNEL="$1"; shift
MM="$1"; shift
FS="$1"; shift
INIT="$1"; shift
FSCK="$1"; shift
OUT_IMG="$1"; shift || true

err() { echo "error: $*" >&2; exit 1; }

[[ -x "$BUILDER" ]] || err "builder not executable: $BUILDER"
for f in "$BOOTBLOCK" "$KERNEL" "$MM" "$FS" "$INIT" "$FSCK"; do
  [[ -f "$f" ]] || err "missing input file: $f"
done

# Prepare a temp bootblock that is exactly 512 bytes (pad or truncate).
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
BB_PADDED="$TMP_DIR/bootblock.512"

BB_SIZE=$(stat -f%z "$BOOTBLOCK" 2>/dev/null || stat -c%s "$BOOTBLOCK")
if [[ $BB_SIZE -gt 512 ]]; then
  echo "warning: bootblock >512B ($BB_SIZE); truncating to 512" >&2
  dd if="$BOOTBLOCK" of="$BB_PADDED" bs=512 count=1 conv=sync,noerror status=none
else
  cp "$BOOTBLOCK" "$BB_PADDED"
  if [[ $BB_SIZE -lt 512 ]]; then
    dd if=/dev/zero bs=$((512-BB_SIZE)) count=1 status=none >> "$BB_PADDED"
  fi
fi

mkdir -p "$(dirname "$OUT_IMG")"

"$BUILDER" "$BB_PADDED" "$KERNEL" "$MM" "$FS" "$INIT" "$FSCK" "$OUT_IMG"

echo "Image created: $OUT_IMG"

