#!/usr/bin/env bash
# XINIM CMake build wrapper (pure CMake, no Conan)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

LANE="${1:-i486}"
TARGET="${2:-i486_boot_disk}"

echo "[BUILD] Lane: $LANE"
echo "[BUILD] Target: $TARGET"

cmake --preset "${LANE}-standalone" 2>/dev/null || \
  cmake -B "build/${LANE}/Debug" -G Ninja \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DXINIM_CPU_LANE="$LANE" \
    -DXINIM_ENABLE_WERROR=ON

cmake --build "build/${LANE}/Debug" --target "$TARGET"
echo "[BUILD] Done: build/${LANE}/Debug/"
