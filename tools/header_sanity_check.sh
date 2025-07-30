#!/bin/bash
## \file header_sanity_check.sh
## \brief Ensure VM test sources compile with provided headers only.
##
## This script compiles each C source file in tests/vm with strict include
## paths to verify headers do not leak undeclared dependencies.

set -euo pipefail
CC=${CC:-gcc}
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Include directories as an array to handle spaces correctly
INCLUDE_PATHS=(
    "$REPO_ROOT/sys"
    "$REPO_ROOT/sys/vm/include"
)

for src in "$REPO_ROOT"/tests/vm/*.c; do
    [ -e "$src" ] || continue
    echo "Checking $src"
    "$CC" -fsyntax-only "${INCLUDE_PATHS[@]/#/-idirafter }" "$src"
    echo "OK"
done
