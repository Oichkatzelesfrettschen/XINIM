#!/usr/bin/env bash
## \file run_qemu.sh
## \brief Launch XINIM disk image under QEMU.
##
## Usage: ./tools/run_qemu.sh <disk_image>
## This script boots the specified disk image using qemu-system-x86_64
## with sane defaults. Additional arguments are passed directly to QEMU.

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <disk_image> [extra_qemu_args...]" >&2
    exit 1
fi

IMAGE="$1"
shift

qemu-system-x86_64 \
    -drive file="${IMAGE}",if=none,format=raw,id=hd0 \
    -device ich9-ahci,id=ahci \
    -device ide-hd,drive=hd0,bus=ahci.0 \
    -serial mon:stdio \
    -display none \
    "$@"
