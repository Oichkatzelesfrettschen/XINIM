#!/bin/sh
# Verify that the generated 32-bit boot disk carries the persistent root seed.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
export XINIM_REPO_ROOT="${PROJECT_ROOT}"
# shellcheck disable=SC1091
. "${PROJECT_ROOT}/scripts/xinim-env.sh"
xinim_ensure_project_dirs

LANE_NAME="${XINIM_BOOT_LANE_NAME:-i486}"
BOOT_DISK="${XINIM_QEMU_BOOT_DISK:-${XINIM_IMAGE_ROOT}/${LANE_NAME}/xinim-${LANE_NAME}-boot.vmdk}"

if [ ! -f "$BOOT_DISK" ]; then
    echo "SKIP: Boot disk not found: $BOOT_DISK"
    exit 77
fi

if ! command -v guestfish >/dev/null 2>&1; then
    echo "SKIP: guestfish not found"
    exit 77
fi

if ! command -v qemu-img >/dev/null 2>&1; then
    echo "SKIP: qemu-img not found"
    exit 77
fi

check_disk() {
    image="$1"
    label="$2"

    if [ ! -f "$image" ]; then
        echo "SKIP: ${label} image not found: $image"
        exit 77
    fi

    qemu-img check "$image" >/dev/null

    listing="$(
        guestfish --ro -a "$image" \
            run : \
            list-filesystems : \
            mount-ro /dev/sda1 / : \
            ls /etc : \
            cat /etc/persist.txt : \
            cat /etc/issue : \
            cat /etc/persist-profile : \
            cat /var/disk-marker 2>&1
    )" || {
        echo "FAIL: guestfish could not inspect ${label} image"
        echo "$listing"
        exit 1
    }

    echo "$listing" | grep -q "/dev/sda1: ext2" || {
        echo "FAIL: ${label} image missing ext2 partition"
        echo "$listing"
        exit 1
    }

    echo "$listing" | grep -q "persist.txt" || {
        echo "FAIL: ${label} image missing /etc/persist.txt directory entry"
        echo "$listing"
        exit 1
    }

    echo "$listing" | grep -q "persistent-root-ok" || {
        echo "FAIL: ${label} image missing persistent root marker"
        echo "$listing"
        exit 1
    }

    echo "$listing" | grep -q "persistent ext2 root" || {
        echo "FAIL: ${label} image missing /etc/issue persistent-root text"
        echo "$listing"
        exit 1
    }

    echo "$listing" | grep -q "PERSIST_PROFILE=disk-root" || {
        echo "FAIL: ${label} image missing persistent profile"
        echo "$listing"
        exit 1
    }

    echo "$listing" | grep -q "ata-ext2-ready" || {
        echo "FAIL: ${label} image missing disk marker"
        echo "$listing"
        exit 1
    }
}

check_disk "$BOOT_DISK" "VMDK"

case "$BOOT_DISK" in
    *.vmdk) QCOW2_DISK="${BOOT_DISK%.vmdk}.qcow2" ;;
    *) QCOW2_DISK="" ;;
esac

if [ -n "$QCOW2_DISK" ] && [ -f "$QCOW2_DISK" ]; then
    check_disk "$QCOW2_DISK" "qcow2"
fi

echo "PASS: ${LANE_NAME} boot disk layout test"
