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

case "$BOOT_DISK" in
    *.vmdk) QCOW2_DISK="${BOOT_DISK%.vmdk}.qcow2" ;;
    *) QCOW2_DISK="" ;;
esac

set -- --ro -a "$BOOT_DISK"
DEVICES="/dev/sda1"
qemu-img check "$BOOT_DISK" >/dev/null
if [ -n "$QCOW2_DISK" ] && [ -f "$QCOW2_DISK" ]; then
    qemu-img check "$QCOW2_DISK" >/dev/null
    set -- "$@" -a "$QCOW2_DISK"
    DEVICES="$DEVICES /dev/sdb1"
fi

# One appliance inspects each image independently without repeating firmware boot.
inspection="$(
    {
        printf 'run\nlist-filesystems\n'
        for device in $DEVICES; do
            printf 'echo XINIM_DISK_BEGIN %s\nmount-ro %s /\n' "$device" "$device"
            printf 'ls /etc\ncat /etc/persist.txt\ncat /etc/issue\n'
            printf 'cat /etc/persist-profile\ncat /var/disk-marker\numount-all\n'
            printf 'echo XINIM_DISK_END %s\n' "$device"
        done
    } | guestfish "$@" 2>&1
)" || {
    echo "FAIL: guestfish could not inspect boot images"
    echo "$inspection"
    exit 1
}

check_disk() {
    device="$1"
    label="$2"
    listing="$(printf '%s\n' "$inspection" | awk -v device="$device" '
        $0 == "XINIM_DISK_BEGIN " device { inside = 1; next }
        $0 == "XINIM_DISK_END " device { inside = 0 }
        inside { print }
    ')"

    printf '%s\n' "$inspection" | grep -qx "$device: ext2" || {
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

check_disk /dev/sda1 "VMDK"

if [ -n "$QCOW2_DISK" ] && [ -f "$QCOW2_DISK" ]; then
    check_disk /dev/sdb1 "qcow2"
fi

echo "PASS: ${LANE_NAME} boot disk layout test"
