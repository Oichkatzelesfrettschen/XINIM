#!/usr/bin/env bash
# Simple QEMU launcher for XINIM disk images.
# Usage: ./run_qemu.sh <path-to-image>
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <disk-image>" >&2
  exit 1
fi
img="$1"
if [[ ! -f "$img" ]]; then
  echo "run_qemu.sh: image '$img' not found" >&2
  exit 1
fi
qemu-system-x86_64 \
  -drive file="$img",if=none,format=raw,id=hd \
  -device virtio-blk-pci,drive=hd \
  -m 512M -smp 2 -serial stdio

