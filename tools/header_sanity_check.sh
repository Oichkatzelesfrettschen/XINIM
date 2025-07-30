#!/usr/bin/env bash
# ------------------------------------------------------------------------------
# header_sanity_check.sh
# This script verifies that VM test sources compile with the correct
# include paths. It is resilient to spaces in the repository path by
# using an array for compiler flags.
# ------------------------------------------------------------------------------
set -euo pipefail
CC=${CC:-gcc}
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INCLUDE_PATHS=("$REPO_ROOT/sys" "$REPO_ROOT/sys/vm/include")

for src in "$REPO_ROOT"/tests/vm/*.c; do
    [ -e "$src" ] || continue
    echo "Checking $src"
    args=()
    for inc in "${INCLUDE_PATHS[@]}"; do
        args+=("-idirafter" "$inc")
    done
    "$CC" -fsyntax-only "${args[@]}" "$src"
done
