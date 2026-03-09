#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
export XINIM_REPO_ROOT="$(dirname "${SCRIPT_DIR}")"
source "${SCRIPT_DIR}/xinim-env.sh"
xinim_ensure_project_dirs

ARG1="${1:-}"
ARG2="${2:-}"
PROFILE="${3:-}"

case "${ARG1}" in
    Debug|debug|Release|release|RelWithDebInfo|MinSizeRel)
        BUILD_TYPE="${ARG1}"
        OUTPUT_DIR="${ARG2:-}"
        ;;
    *)
        OUTPUT_DIR="${ARG1}"
        BUILD_TYPE="${ARG2:-Debug}"
        ;;
esac

case "${BUILD_TYPE}" in
    Debug|debug) BUILD_DIR_NAME="Debug" ;;
    Release|release) BUILD_DIR_NAME="Release" ;;
    RelWithDebInfo) BUILD_DIR_NAME="RelWithDebInfo" ;;
    MinSizeRel) BUILD_DIR_NAME="MinSizeRel" ;;
    *)
        BUILD_DIR_NAME="${BUILD_TYPE}"
        ;;
esac

if [[ -z "${OUTPUT_DIR}" || "${OUTPUT_DIR}" == "." ]]; then
    OUTPUT_DIR="${XINIM_BUILD_ROOT}/${BUILD_DIR_NAME}"
fi

conan profile detect --force

PROFILE_ARGS=()
if [[ -z "$PROFILE" && -f "conan/profiles/xinim-clang" ]]; then
    PROFILE="conan/profiles/xinim-clang"
fi
if [[ -n "$PROFILE" ]]; then
    PROFILE_ARGS=(-pr "$PROFILE")
fi

conan install . -s build_type="${BUILD_TYPE}" -of "${OUTPUT_DIR}" --build=missing "${PROFILE_ARGS[@]}"

# Keep the working tree clean: we use the repo's checked-in presets plus the
# external generators directory, so Conan's repo-local user presets are noise.
rm -f CMakeUserPresets.json
