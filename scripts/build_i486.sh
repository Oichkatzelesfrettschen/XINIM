#!/usr/bin/env bash
# XINIM i486 lane -- reproducible build entrypoint.
#
# Usage:
#   ./scripts/build_i486.sh          # Build kernel + bootable disk
#   ./scripts/build_i486.sh --clean  # Clean build from scratch
#   ./scripts/build_i486.sh --run    # Build + launch QEMU
#   ./scripts/build_i486.sh --test   # Build + run VBox test suite
#
# Prerequisites:
#   - CMake 3.28+, Ninja, Clang 18+, GCC (for dietlibc cross-compile)
#   - grub-mkimage, mke2fs, qemu-img
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_ROOT}/build/i486/Debug"

if [[ "${1:-}" == "--clean" ]]; then
    echo "Cleaning build..."
    rm -rf "$BUILD_DIR"
    shift
fi

echo "Configuring..."
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --preset i486-standalone

echo "Building kernel + disk..."
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build "$BUILD_DIR" --target i486_boot_disk

echo "Build complete:"
ls -lh "$BUILD_DIR/images/i486/xinim-i486-boot.vmdk" "$BUILD_DIR/images/i486/xinim-i486-boot.qcow2" 2>/dev/null

if [[ "${1:-}" == "--run" ]]; then
    exec "$SCRIPT_DIR/qemu_i486.sh"
elif [[ "${1:-}" == "--test" ]]; then
    exec "$SCRIPT_DIR/test_i486_vbox.sh"
fi
