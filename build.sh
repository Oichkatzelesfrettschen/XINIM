#!/usr/bin/env bash
#
# build.sh - orchestrate XINIM builds with profile-specific optimizations.
#
# This script wraps CMake/Ninja invocations to provide coherent build
# configurations tailored for development, performance analysis, and
# production release. Each profile configures dedicated output directories
# and carefully tuned compiler flags.
#
# Usage:
#   ./build.sh [profile]
#   ./build.sh --help
#
# Profiles:
#   developer   Debug build with sanitizers and extensive diagnostics.
#   performance Profiling-oriented build with moderate optimizations.
#   release     Production build with aggressive optimizations.
#
set -euo pipefail

usage() {
	cat <<USAGE
Usage: $(basename "$0") <profile>

Profiles:
  developer   Debug build with sanitizers and diagnostics.
  performance Optimized build for profiling.
  release     Production build with maximum optimization.

Options:
  -h, --help  Show this help message.
USAGE
}

if [[ $# -ne 1 ]]; then
	usage
	exit 1
fi

case "$1" in
-h | --help)
	usage
	exit 0
	;;
developer)
	BUILD_DIR="build/developer"
	CMAKE_BUILD_TYPE="Debug"
	CXXFLAGS="-O0 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined"
	;;
performance)
	BUILD_DIR="build/performance"
	CMAKE_BUILD_TYPE="RelWithDebInfo"
	CXXFLAGS="-O2 -g -march=native -mtune=native -DNDEBUG"
	;;
release)
	BUILD_DIR="build/release"
	CMAKE_BUILD_TYPE="Release"
	CXXFLAGS="-O3 -flto -march=native -mtune=native -DNDEBUG"
	;;
*)
	echo "Unknown profile: $1" >&2
	usage
	exit 1
	;;
esac

cmake -G Ninja -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" -DCMAKE_CXX_FLAGS="$CXXFLAGS"
cmake --build "$BUILD_DIR" --parallel
