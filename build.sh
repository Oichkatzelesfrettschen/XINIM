#!/usr/bin/env bash
#
# build.sh - Unified CMake build driver for XINIM
#
# This script configures and builds the project using preset profiles
# tuned for development, benchmarking, or release packaging. It wraps
# standard CMake invocations while enforcing a consistent tool-chain
# (Clang/LLVM 18 and Ninja) across environments.
#
# Usage:
#   ./build.sh <profile> [additional CMake options]
#   ./build.sh --help
#
# Profiles:
#   developer   - Debug build with sanitizers and extensive warnings.
#   performance - Release build optimized for the host CPU.
#   release     - Generic release build with link-time optimization.
#
# Dependencies:
#   * CMake ≥ 3.5
#   * Ninja
#   * clang-18 tool suite (clang++, lld, etc.)
#
# Any extra arguments following the profile are forwarded directly to
# CMake, enabling fine-grained configuration when necessary.

set -euo pipefail
IFS=$'\n\t'

show_help() {
  grep '^#' "$0" | tail -n +2 | sed -E 's/^# ?//'
  exit 0
}

if [[ ${1:-} =~ ^(-h|--help)$ ]]; then
  show_help
fi

if [[ $# -lt 1 ]]; then
  echo "Error: no profile specified" >&2
  show_help
fi

profile=$1
shift

CMAKE_COMMON=(
  -S .
  -G Ninja
  -DCMAKE_C_COMPILER=clang-18
  -DCMAKE_CXX_COMPILER=clang++-18
)

case "$profile" in
  developer)
    CMAKE_FLAGS=(
      -B build/developer
      -DCMAKE_BUILD_TYPE=Debug
      -DCMAKE_C_FLAGS_DEBUG="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer"
      -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer"
    )
    ;;
  performance)
    CMAKE_FLAGS=(
      -B build/performance
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto"
      -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto"
    )
    ;;
  release)
    CMAKE_FLAGS=(
      -B build/release
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
      -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG -flto"
      -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG -flto"
    )
    ;;
  *)
    echo "Error: unknown profile '$profile'" >&2
    show_help
    ;;
 esac

cmake "${CMAKE_COMMON[@]}" "${CMAKE_FLAGS[@]}" "$@"
cmake --build "build/$profile"
