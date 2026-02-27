#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${1:-.}"
BUILD_TYPE="${2:-Debug}"
PROFILE="${3:-}"

conan profile detect --force

PROFILE_ARGS=()
if [[ -z "$PROFILE" && -f "conan/profiles/xinim-clang" ]]; then
    PROFILE="conan/profiles/xinim-clang"
fi
if [[ -n "$PROFILE" ]]; then
    PROFILE_ARGS=(-pr "$PROFILE")
fi

conan install . -s build_type="${BUILD_TYPE}" -of "${OUTPUT_DIR}" --build=missing "${PROFILE_ARGS[@]}"
