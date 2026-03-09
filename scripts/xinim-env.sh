#!/usr/bin/env sh
# Shared XINIM path layout for self-contained repo-local builds and artifacts.

if [ -n "${XINIM_REPO_ROOT:-}" ]; then
    REPO_ROOT="${XINIM_REPO_ROOT}"
else
    REPO_ROOT=$(pwd)
fi

if [ -z "${XINIM_PROJECT_ROOT:-}" ]; then
    XINIM_PROJECT_ROOT="${REPO_ROOT}"
fi

if [ -z "${XINIM_STATE_ROOT:-}" ]; then
    XINIM_STATE_ROOT="${XINIM_PROJECT_ROOT}/build/_state"
fi

: "${XINIM_BUILD_ROOT:=${XINIM_PROJECT_ROOT}/build}"
: "${XINIM_IMAGE_ROOT:=${XINIM_BUILD_ROOT}/images}"
: "${XINIM_LOG_ROOT:=${XINIM_BUILD_ROOT}/logs}"
: "${XINIM_CACHE_ROOT:=${XINIM_STATE_ROOT}/cache}"
: "${XINIM_DOWNLOAD_ROOT:=${XINIM_STATE_ROOT}/downloads}"
: "${XINIM_PREFIX:=${XINIM_STATE_ROOT}/toolchain}"
: "${XINIM_SYSROOT:=${XINIM_STATE_ROOT}/sysroot}"
: "${XINIM_VENV_ROOT:=${XINIM_STATE_ROOT}/venv}"
: "${XINIM_TOOLS_ROOT:=${XINIM_STATE_ROOT}/tools}"
: "${CONAN_HOME:=${XINIM_CACHE_ROOT}/conan/home}"

export XINIM_STATE_ROOT
export XINIM_PROJECT_ROOT
export XINIM_BUILD_ROOT
export XINIM_IMAGE_ROOT
export XINIM_LOG_ROOT
export XINIM_CACHE_ROOT
export XINIM_DOWNLOAD_ROOT
export XINIM_PREFIX
export XINIM_SYSROOT
export XINIM_VENV_ROOT
export XINIM_TOOLS_ROOT
export CONAN_HOME

xinim_ensure_project_dirs() {
    mkdir -p \
        "${XINIM_STATE_ROOT}" \
        "${XINIM_BUILD_ROOT}" \
        "${XINIM_IMAGE_ROOT}" \
        "${XINIM_LOG_ROOT}" \
        "${XINIM_CACHE_ROOT}" \
        "${XINIM_DOWNLOAD_ROOT}" \
        "${XINIM_PREFIX}" \
        "${XINIM_SYSROOT}" \
        "${XINIM_VENV_ROOT}" \
        "${XINIM_TOOLS_ROOT}" \
        "${CONAN_HOME}"
}
