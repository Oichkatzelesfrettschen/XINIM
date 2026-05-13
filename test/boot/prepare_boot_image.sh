#!/bin/sh
# Prepare a named boot image target for CTest fixtures.

set -eu

if [ $# -ne 1 ]; then
    echo "usage: $0 <cmake-target>" >&2
    exit 2
fi

TARGET_NAME="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

BUILD_DIR="${XINIM_ACTIVE_BUILD_DIR:-${XINIM_BUILD_ROOT:-${PROJECT_ROOT}/build/x86_64/Debug}}"
TOOLS_ROOT="${XINIM_TOOLS_ROOT:-${BUILD_DIR}/tools}"

case "${TARGET_NAME}" in
    xinim_x86_64_image)
        if [ ! -x "${TOOLS_ROOT}/limine/v10.8.3-binary/limine" ]; then
            echo "SKIP: Limine assets not bootstrapped under ${TOOLS_ROOT}"
            exit 77
        fi
        ;;
    xinim_*_image)
        if [ -n "${XINIM_QEMU_DISK_IMAGE:-}" ] && [ -f "${XINIM_QEMU_DISK_IMAGE}" ]; then
            rm -f "${XINIM_QEMU_DISK_IMAGE}"
        fi
        :
        ;;
    *)
        echo "Unknown boot image target: ${TARGET_NAME}" >&2
        exit 2
        ;;
esac

cmake --build "${BUILD_DIR}" --target "${TARGET_NAME}"
