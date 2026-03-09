#!/bin/sh
# Generic XINIM 32-bit boot smoke test.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
# shellcheck disable=SC1091
export XINIM_REPO_ROOT="${PROJECT_ROOT}"
. "${PROJECT_ROOT}/scripts/xinim-env.sh"
xinim_ensure_project_dirs

LANE_NAME="${XINIM_BOOT_LANE_NAME:-i486}"
case "${LANE_NAME}" in
    i486) IMAGE_NAME="xinim-i486dx.iso" ;;
    i586) IMAGE_NAME="xinim-i586.iso" ;;
    i686) IMAGE_NAME="xinim-i686.iso" ;;
    x86_32_core2) IMAGE_NAME="xinim-x86-32-core2.iso" ;;
    x86_32_athlon) IMAGE_NAME="xinim-x86-32-athlon.iso" ;;
    x86_32_phenom) IMAGE_NAME="xinim-x86-32-phenom.iso" ;;
    *) IMAGE_NAME="xinim-${LANE_NAME}.iso" ;;
esac
LANE_BANNER="${XINIM_BOOT_LANE_BANNER:-${LANE_NAME}}"
BOOT_IMAGE="${XINIM_QEMU_BOOT_IMAGE:-${XINIM_IMAGE_ROOT}/${LANE_NAME}/${IMAGE_NAME}}"
QEMU_BIN="${XINIM_QEMU_SYSTEM_BIN:-qemu-system-i386}"
QEMU_MACHINE="${XINIM_QEMU_MACHINE:-pc}"
case "${LANE_NAME}" in
    i486)
        DEFAULT_QEMU_CPU="486"
        DEFAULT_QEMU_MEMORY="32M"
        ;;
    i586)
        DEFAULT_QEMU_CPU="pentium"
        DEFAULT_QEMU_MEMORY="48M"
        ;;
    i686)
        DEFAULT_QEMU_CPU="pentium3"
        DEFAULT_QEMU_MEMORY="64M"
        ;;
    *)
        DEFAULT_QEMU_CPU="486"
        DEFAULT_QEMU_MEMORY="32M"
        ;;
esac
QEMU_CPU="${XINIM_QEMU_CPU:-${DEFAULT_QEMU_CPU}}"
QEMU_MEMORY="${XINIM_QEMU_MEMORY:-${DEFAULT_QEMU_MEMORY}}"
QEMU_VGA="${XINIM_QEMU_VGA:-std}"
TIMEOUT_SEC="${XINIM_QEMU_TIMEOUT_SEC:-8}"
LOG_FILE="${XINIM_QEMU_SMOKE_LOG:-${XINIM_LOG_ROOT}/${LANE_NAME}-smoke.log}"

if [ ! -f "$BOOT_IMAGE" ]; then
    echo "SKIP: Boot image not found: $BOOT_IMAGE"
    exit 77
fi

rm -f "$LOG_FILE"

"${QEMU_BIN}" \
    -machine "${QEMU_MACHINE}" \
    -cpu "${QEMU_CPU}" \
    -m "${QEMU_MEMORY}" \
    -boot d \
    -cdrom "${BOOT_IMAGE}" \
    -vga "${QEMU_VGA}" \
    -display none \
    -serial "file:${LOG_FILE}" \
    -monitor none \
    -no-reboot \
    -no-shutdown &
QEMU_PID=$!

sleep "$TIMEOUT_SEC"
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

if [ ! -f "$LOG_FILE" ]; then
    echo "FAIL: No serial output captured"
    exit 1
fi

grep -q "XINIM ${LANE_BANNER} Booting" "$LOG_FILE" || {
    echo "FAIL: Missing ${LANE_BANNER} boot banner"
    exit 1
}

grep -q "boot protocol: multiboot2" "$LOG_FILE" || {
    echo "FAIL: Missing multiboot2 handoff confirmation"
    exit 1
}

grep -q "Launching Ring 3 xash" "$LOG_FILE" || {
    echo "FAIL: Missing Ring 3 launch marker"
    exit 1
}

echo "PASS: ${LANE_NAME} boot smoke test"
