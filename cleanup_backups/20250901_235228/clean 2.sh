#!/usr/bin/env bash
## \file clean.sh
## \brief Remove build artifacts and generated binaries.
##
## This script deletes the main build directory produced by CMake and
## any compiled tools placed under tools/build. Run it from the repository
## root to reset the workspace.

set -euo pipefail

remove_dir() {
    local dir="$1"
    if [[ -d "$dir" ]]; then
        rm -rf "$dir"
        echo "Removed $dir"
    fi
}

main() {
    remove_dir "build"
    remove_dir "tools/build"
}

main "$@"
