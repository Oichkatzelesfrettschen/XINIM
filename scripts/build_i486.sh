#!/usr/bin/env bash
# XINIM i486 lane -- reproducible build entrypoint.
#
# Usage:
#   ./scripts/build_i486.sh          # Full build (conan + cmake + ninja)
#   ./scripts/build_i486.sh --clean  # Clean build from scratch
#   ./scripts/build_i486.sh --run    # Build + launch QEMU
#
# Prerequisites:
#   - Conan 2.x, CMake 3.28+, Ninja, Clang 21+
#   - xorriso (for ISO generation)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_ROOT}/build/i486/Debug"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

info() { echo -e "${BLUE}[build]${NC} $1"; }
ok()   { echo -e "${GREEN}[build]${NC} $1"; }
err()  { echo -e "${RED}[build]${NC} $1"; }

CLEAN=0
RUN=0
for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=1 ;;
        --run)   RUN=1 ;;
        -h|--help)
            echo "Usage: $0 [--clean] [--run]"
            echo "  --clean  Remove build directory and rebuild from scratch"
            echo "  --run    Launch QEMU after successful build"
            exit 0
            ;;
    esac
done

cd "${PROJECT_ROOT}"

if [[ "${CLEAN}" == "1" ]]; then
    info "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

# Step 1: Conan install (generates toolchain)
if [[ ! -f "${BUILD_DIR}/generators/conan_toolchain.cmake" ]]; then
    info "Running conan install..."
    conan install . \
        --output-folder="${BUILD_DIR}" \
        --build=missing \
        -s build_type=Debug \
        -pr:h conan/profiles/clang-x86_32 \
        -o "lane=i486"
    ok "Conan install complete"
fi

# Step 2: CMake configure
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
    info "Running cmake configure..."
    cmake --preset i486-debug
    ok "CMake configure complete"
fi

# Step 3: Build
info "Building..."
cmake --build --preset i486-debug
ok "Build complete"

# Summary
BOOT_IMAGE="${BUILD_DIR}/images/i486/xinim-i486dx.iso"
if [[ -f "${BOOT_IMAGE}" ]]; then
    SIZE_KB=$(( $(stat -f%z "${BOOT_IMAGE}" 2>/dev/null || stat -c%s "${BOOT_IMAGE}") / 1024 ))
    ok "Boot image: ${BOOT_IMAGE} (${SIZE_KB} KB)"
else
    err "Boot image not found (xorriso may not be installed)"
fi

# Step 4: Run QEMU (optional)
if [[ "${RUN}" == "1" ]]; then
    info "Launching QEMU..."
    exec "${SCRIPT_DIR}/qemu_i486.sh" \
        --boot-image "${BOOT_IMAGE}" \
        --disk-image "${BUILD_DIR}/images/i486/xinim-i486-ata.img"
fi
