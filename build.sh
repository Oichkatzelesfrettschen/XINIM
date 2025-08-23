#!/usr/bin/env bash
## \file build.sh
## \brief Unified build driver for the XINIM project.
##
## This script configures and builds the project using CMake and Ninja.
## It exposes selectable build profiles tuned for typical development
## scenarios and documents the controlling environment variables.
set -euo pipefail

usage() {
	cat <<'USAGE'
Usage: ./build.sh [--profile=developer|performance|release] [--build-dir=DIR] [--] [extra CMake flags]

Profiles:
  developer   Debug build with sanitizers and verbose warnings.
  performance Build with profiling instrumentation enabled.
  release     Optimized build with link-time optimization and no asserts.

Environment variables:
  CMAKE     Path to the cmake executable (default: cmake).
  NINJA     Path to the ninja executable (default: ninja).
  BUILD_DIR Build directory location (default: build).
USAGE
}

profile="developer"
build_dir=${BUILD_DIR:-build}
cmake_cmd=${CMAKE:-cmake}
ninja_cmd=${NINJA:-ninja}
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
		usage
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

cmake_flags=(-S . -B "$build_dir" -G Ninja)

case "$profile" in
developer)
	cmake_flags+=(
		-DCMAKE_BUILD_TYPE=Debug
		-DCMAKE_CXX_FLAGS="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer"
	)
	;;
performance)
	cmake_flags+=(
		-DCMAKE_BUILD_TYPE=RelWithDebInfo
		-DCMAKE_CXX_FLAGS="-O2 -g -pg"
	)
	;;
release)
	cmake_flags+=(
		-DCMAKE_BUILD_TYPE=Release
		-DCMAKE_CXX_FLAGS="-O3 -DNDEBUG -flto"
	)
	;;
*)
	echo "Unknown profile: $profile" >&2
	usage
	exit 1
	;;
esac

if [ ${#cmake_extra[@]} -gt 0 ]; then
	cmake_flags+=("${cmake_extra[@]}")
fi

"$cmake_cmd" "${cmake_flags[@]}"
"$ninja_cmd" -C "$build_dir"

printf 'Build artifacts located at: %s\n' "$(realpath "$build_dir")"
