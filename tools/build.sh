#!/usr/bin/env bash
# Unified build script for XINIM.
# Usage: ./build.sh [generic|performance|developer]
# This wraps the CMake-based build with common options.
set -euo pipefail
mode="${1:-generic}"
tests="${TESTS:-OFF}"
case "$mode" in
  generic)
    type=RelWithDebInfo
    cflags="-Werror"
    ;;
  performance)
    type=Release
    cflags="-O3 -flto -Werror"
    ;;
  developer)
    type=Debug
    cflags="-g3 -fsanitize=address,undefined -Werror"
    ;;
  *)
    echo "Usage: $0 [generic|performance|developer]" >&2
    exit 1
    ;;
esac
CC=clang-18 CXX=clang++-18 cmake -B build -DCMAKE_BUILD_TYPE="$type" \
  -DCMAKE_C_FLAGS="$cflags" -DCMAKE_CXX_FLAGS="$cflags" \
  -DENABLE_UNIT_TESTS=$tests
cmake --build build -j$(nproc)
