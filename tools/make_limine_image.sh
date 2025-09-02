#!/usr/bin/env bash
# Build a bootable Limine disk image with XINIM kernel.
# Requirements: limine host tool & binaries (0BSD), mtools (for FAT image), qemu (to test).

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build"
LIMINE_DIR="$ROOT/third_party/limine-codeberg"
LIMINE_PROTO="$ROOT/third_party/limine-protocol/include/limine.h"

KERNEL="$BUILD/xinim.kernel"
CFG="$ROOT/boot/limine/limine.cfg"
IMG="$BUILD/xinim-limine.img"

usage() {
  cat <<EOF
Usage: $0 [--limine <path-to-limine>] [--size-mb 64]

Builds a FAT32 disk image with Limine and XINIM kernel at $IMG.
EOF
}

LIMINE_BIN=""
SIZE_MB=64
while [[ $# -gt 0 ]]; do
  case "$1" in
    --limine) LIMINE_BIN="$2"; shift 2;;
    --size-mb) SIZE_MB="$2"; shift 2;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown arg: $1" >&2; usage; exit 2;;
  esac
done

mkdir -p "$BUILD"

if [[ ! -f "$KERNEL" ]]; then
  echo "error: kernel not found at $KERNEL (build with -DBUILD_KERNEL=ON)" >&2; exit 1
fi

# Locate Limine host tool if not provided
if [[ -z "$LIMINE_BIN" ]]; then
  if command -v limine >/dev/null 2>&1; then
    LIMINE_BIN="$(command -v limine)"
  elif [[ -x "$LIMINE_DIR/limine" ]]; then
    LIMINE_BIN="$LIMINE_DIR/limine"
  else
    echo "warning: limine host tool not found. Attempting to use binaries only."
  fi
fi

# Create empty image and format with FAT
dd if=/dev/zero of="$IMG" bs=1M count=$SIZE_MB status=none
parted -s "$IMG" mklabel gpt mkpart EFI fat32 1MiB 100% set 1 esp on || {
  echo "parted not available; trying mformat directly (whole image)";
}

# Use mtools to create a FAT filesystem and copy files
MTOOLSRC=$(mktemp)
trap 'rm -f "$MTOOLSRC"' EXIT
cat > "$MTOOLSRC" <<EOF
drive l: file="$IMG" fat_bits=32 mformat_only
EOF
export MTOOLSRC

mformat l: || true
echo "Copying kernel and limine.cfg and UEFI BOOTX64.EFI"
mkdir -p "$BUILD/mnt"
mpath=$(mktemp -d)
trap 'rm -rf "$mpath"' EXIT
cp "$KERNEL" "$mpath/xinim.kernel"
cp "$CFG" "$mpath/limine.cfg"
# UEFI fallback path
mkdir -p "$mpath/EFI/BOOT"
if [[ -f "$LIMINE_DIR/BOOTX64.EFI" ]]; then
  cp "$LIMINE_DIR/BOOTX64.EFI" "$mpath/EFI/BOOT/BOOTX64.EFI"
else
  echo "warning: BOOTX64.EFI not found; UEFI boot may not work" >&2
fi
mcopy -s "$mpath"/* l:/

# Copy Limine BIOS files if present
for f in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin; do
  if [[ -f "$LIMINE_DIR/$f" ]]; then mcopy "$LIMINE_DIR/$f" l:/; fi
done

# Deploy bootloader if tool exists
if [[ -n "$LIMINE_BIN" ]]; then
  "$LIMINE_BIN" deploy "$IMG"
else
  echo "warning: limine tool not found, skipping deploy. Image may not boot."
fi

echo "Image created: $IMG"
