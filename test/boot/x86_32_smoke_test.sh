#!/bin/sh
# Generic XINIM 32-bit supervised-init boot smoke test.

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
QEMU_DISK_IMAGE="${XINIM_QEMU_DISK_IMAGE:-}"
TIMEOUT_SEC="${XINIM_QEMU_TIMEOUT_SEC:-12}"
LOG_FILE="${XINIM_QEMU_SMOKE_LOG:-${XINIM_LOG_ROOT}/${LANE_NAME}-smoke.log}"

if [ ! -f "$BOOT_IMAGE" ]; then
    echo "SKIP: Boot image not found: $BOOT_IMAGE"
    exit 77
fi

rm -f "$LOG_FILE"

set -- \
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
    -no-shutdown

if [ -n "${QEMU_DISK_IMAGE}" ] && [ -f "${QEMU_DISK_IMAGE}" ]; then
    set -- "$@" -drive "file=${QEMU_DISK_IMAGE},format=raw,index=0,media=disk"
fi

"${QEMU_BIN}" "$@" &
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

grep -q "Launching supervised Ring 3 services under timer scheduler" "$LOG_FILE" || {
    echo "FAIL: Missing timer-scheduled supervised launch marker"
    exit 1
}

grep -q "Prepared supervised support service hold-service" "$LOG_FILE" || {
    echo "FAIL: Missing supervised support-service preparation marker"
    exit 1
}

if [ -n "${QEMU_DISK_IMAGE}" ] && [ -f "${QEMU_DISK_IMAGE}" ]; then
    grep -q "ATA primary master: present" "$LOG_FILE" || {
        echo "FAIL: Missing ATA primary master detection"
        exit 1
    }

    grep -q "ATA primary master sector0: XINIMHD0 mbr=yes" "$LOG_FILE" || {
        echo "FAIL: Missing ATA sector0 evidence marker"
        exit 1
    }

    grep -q "ATA primary master partition1: type=0x00000083 start=2048 sectors=30720" "$LOG_FILE" || {
        echo "FAIL: Missing ATA partition probe evidence"
        exit 1
    }

    grep -q "ATA primary master ext2: block_size=1024 blocks=15360 inodes=" "$LOG_FILE" || {
        echo "FAIL: Missing ext2 superblock probe evidence"
        exit 1
    }

    grep -q "ext2 reader: ready" "$LOG_FILE" || {
        echo "FAIL: Missing ext2 reader ready marker"
        exit 1
    }

    grep -q "ext2 /etc/persist.txt" "$LOG_FILE" || {
        echo "FAIL: Missing ext2 persist file path probe"
        exit 1
    }

    grep -q "persistent-root-ok." "$LOG_FILE" || {
        echo "FAIL: Missing ext2 persist file probe"
        exit 1
    }

    grep -q "ext2 mount: ready path=/persist" "$LOG_FILE" || {
        echo "FAIL: Missing ext2 mount registration evidence"
        exit 1
    }
fi

echo "PASS: ${LANE_NAME} boot smoke test"
