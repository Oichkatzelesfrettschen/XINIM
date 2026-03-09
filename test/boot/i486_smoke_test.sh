#!/bin/sh
# Compatibility wrapper for the generic 32-bit smoke test.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export XINIM_BOOT_LANE_NAME="${XINIM_BOOT_LANE_NAME:-i486}"
export XINIM_BOOT_LANE_BANNER="${XINIM_BOOT_LANE_BANNER:-i486}"
export XINIM_QEMU_SYSTEM_BIN="${XINIM_QEMU_SYSTEM_BIN:-qemu-system-i386}"
export XINIM_QEMU_CPU="${XINIM_QEMU_CPU:-486}"
export XINIM_QEMU_MACHINE="${XINIM_QEMU_MACHINE:-pc}"
export XINIM_QEMU_MEMORY="${XINIM_QEMU_MEMORY:-32M}"
export XINIM_QEMU_VGA="${XINIM_QEMU_VGA:-std}"
exec "${SCRIPT_DIR}/x86_32_smoke_test.sh"
