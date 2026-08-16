#!/bin/bash
# XINIM QEMU Interactive Test Script
# Targets the current x86_64 image-based lane with serial/telnet interaction.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck disable=SC1091
export XINIM_REPO_ROOT="${PROJECT_ROOT}"
source "${SCRIPT_DIR}/xinim-env.sh"
xinim_ensure_project_dirs

BOOT_IMAGE="${XINIM_QEMU_BOOT_IMAGE:-${XINIM_IMAGE_ROOT}/x86_64/xinim-x86_64.iso}"

if [[ ! -f "${BOOT_IMAGE}" ]]; then
    echo "Error: Boot image not found at ${BOOT_IMAGE}"
    echo "Please build an x86_64 boot image first."
    exit 1
fi

echo "Starting XINIM in QEMU (x86_64 v1)..."
echo "==========================================="
echo "Serial Console (COM1): Local Terminal"
echo "Telnet Console (COM2): localhost:4444"
echo "QEMU Monitor:          localhost:4445"
echo "==========================================="

# QEMU Optimized Configuration for Stability & Performance
# -cpu qemu64: Standard x86_64 v1
# -accel kvm: Use KVM if available
# -m 512: Stable memory baseline
# -serial stdio: Interactive shell on COM1
# -serial telnet:localhost:4444,server,nowait: Interactive shell on COM2 (mapped to shell() if implemented)
# -monitor telnet:localhost:4445,server,nowait: Programmatic monitor access

QEMU_ARGS=(
    -machine pc-q35-11.1
    -cpu qemu64
    -m 512M
    -cdrom "${BOOT_IMAGE}"
    -boot d
    -serial stdio
    -serial telnet:localhost:4444,server,nowait
    -monitor telnet:localhost:4445,server,nowait
    -nodefaults
    -vga none
    -nic none
    -display none
    -no-reboot
)

if [[ -e /dev/kvm && -w /dev/kvm ]]; then
    QEMU_ARGS+=(-accel kvm)
else
    QEMU_ARGS+=(-accel tcg)
fi

exec "${XINIM_QEMU_SYSTEM_BIN:-qemu-system-x86_64}" "${QEMU_ARGS[@]}"
