#!/usr/bin/env bash
## \file header_sanity_check.sh
## \brief Compile test VM sources with configured include paths.
##
## This script verifies that all VM related headers compile cleanly under
## the configured compiler. It is meant for quick sanity checks during
## development and continuous integration.

set -euo pipefail

CC=${CC:-gcc}
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Include directories as an array to properly handle spaces in paths
INCLUDES=(
  "-idirafter" "$REPO_ROOT/sys"
  "-idirafter" "$REPO_ROOT/sys/vm/include"
)

shopt -s nullglob
for src in "$REPO_ROOT"/tests/vm/*.c; do
    echo "Checking $src"
    "$CC" -fsyntax-only "${INCLUDES[@]}" "$src"
done

