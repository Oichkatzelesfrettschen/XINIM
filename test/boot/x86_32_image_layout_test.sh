#!/bin/sh
# Verify that a 32-bit lane ISO contains the staged native shell payload.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
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
BOOT_IMAGE="${XINIM_QEMU_BOOT_IMAGE:-${XINIM_IMAGE_ROOT}/${LANE_NAME}/${IMAGE_NAME}}"

if [ ! -f "$BOOT_IMAGE" ]; then
    echo "SKIP: Boot image not found: $BOOT_IMAGE"
    exit 77
fi

if ! command -v xorriso >/dev/null 2>&1; then
    echo "SKIP: xorriso not found"
    exit 77
fi

LISTING="$(xorriso -indev "$BOOT_IMAGE" -find /bin -type f -exec lsdl 2>/dev/null || true)"
BOOT_LISTING="$(xorriso -indev "$BOOT_IMAGE" -find /boot -type f -exec lsdl 2>/dev/null || true)"

echo "$LISTING" | grep -q "/bin/xash" || {
    echo "FAIL: /bin/xash not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$LISTING" | grep -q "/bin/sh" || {
    echo "FAIL: /bin/sh not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$LISTING" | grep -q "/bin/hello" || {
    echo "FAIL: /bin/hello not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$LISTING" | grep -q "/bin/false" || {
    echo "FAIL: /bin/false not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$BOOT_LISTING" | grep -q "/boot/hello" || {
    echo "FAIL: /boot/hello boot module not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$BOOT_LISTING" | grep -q "/boot/false" || {
    echo "FAIL: /boot/false boot module not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$BOOT_LISTING" | grep -q "/boot/xash" || {
    echo "FAIL: /boot/xash boot module not present in ${LANE_NAME} ISO"
    exit 1
}

MOTD_LISTING="$(xorriso -indev "$BOOT_IMAGE" -find /etc -type f -exec lsdl 2>/dev/null || true)"
BOOT_MOTD_LISTING="$(xorriso -indev "$BOOT_IMAGE" -find /boot -type f -exec lsdl 2>/dev/null || true)"

echo "$MOTD_LISTING" | grep -q "/etc/motd" || {
    echo "FAIL: /etc/motd not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$BOOT_MOTD_LISTING" | grep -q "/boot/motd" || {
    echo "FAIL: /boot/motd boot module not present in ${LANE_NAME} ISO"
    exit 1
}

echo "$LISTING" | grep -q "/bin/xinim-sh" && {
    echo "FAIL: deprecated /bin/xinim-sh alias is still present in ${LANE_NAME} ISO"
    exit 1
}

echo "PASS: ${LANE_NAME} image layout test"
