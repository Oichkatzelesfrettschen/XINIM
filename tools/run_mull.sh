#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# run_mull.sh
# -----------------------------------------------------------------------------
# Orchestrates mutation testing for XINIM using Mull.
# The script configures a coverage-enabled build and invokes mull-cxx.
# Requires clang/LLVM 20 and Mull to be installed.
# -----------------------------------------------------------------------------
set -euo pipefail
BUILD_DIR=${BUILD_DIR:-build/mull}
CXX=clang++-20
COV_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
cmake -S tests -B "$BUILD_DIR" -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_CXX_FLAGS="$COV_FLAGS"
cmake --build "$BUILD_DIR" --target minix_test_strlen_property
if command -v mull-cxx >/dev/null 2>&1; then
  mull-cxx --linker "$CXX" -- $BUILD_DIR/minix_test_strlen_property
else
  echo "mull-cxx not found; please install Mull to enable mutation testing." >&2
fi
