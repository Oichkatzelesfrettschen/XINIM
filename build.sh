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
#    ./build.sh --profile=<profile> [--build-dir=DIR] [--] [extra CMake flags]
#    ./build.sh --help
#
# Profiles:
#    developer   - Debug build with sanitizers and extensive warnings.
#    performance - Release build optimized for the host CPU.
#    release     - Generic release build with link-time optimization.
#
# Dependencies:
#    * CMake ≥ 3.5
#    * Ninja
#    * clang-18 tool suite (clang++, lld, etc.)
#
# Any extra arguments following the profile are forwarded directly to
# CMake, enabling fine-grained configuration when necessary.

set -euo pipefail
IFS=$'\n\t'

show_help() {
    grep '^#' "$0" | tail -n +2 | sed -E 's/^# ?//'
    exit 0
}

profile="developer"
build_dir=${BUILD_DIR:-build}
cmake_extra=()

while [ $# -gt 0 ]; do
    case $1 in
    --profile=*)
        profile="${1#*=}"
        ;;
    --build-dir=*)
        build_dir="${1#*=}"
        ;;
    --help | -h)
        show_help
        exit 0
        ;;
    --)
        shift
        cmake_extra=("$@")
        break
        ;;
    *)
        cmake_extra+=("$1")
        ;;
    esac
    shift
done

cmake_flags=(
    -S .
    -G Ninja
    -DCMAKE_C_COMPILER=clang-18
    -DCMAKE_CXX_COMPILER=clang++-18
)

case "$profile" in
    developer)
        cmake_flags+=(
            -B "build/developer"
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_C_FLAGS_DEBUG="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer"
            -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer"
        )
        ;;
    performance)
        cmake_flags+=(
            -B "build/performance"
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto"
            -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto"
        )
        ;;
    release)
        cmake_flags+=(
            -B "build/release"
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
            -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG -flto"
            -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG -flto"
        )
        ;;
    *)
        echo "Error: unknown profile '$profile'" >&2
        show_help
        exit 1
        ;;
esac

if [ ${#cmake_extra[@]} -gt 0 ]; then
    cmake_flags+=("${cmake_extra[@]}")
fi

cmake "${cmake_flags[@]}"
cmake --build "build/$profile"