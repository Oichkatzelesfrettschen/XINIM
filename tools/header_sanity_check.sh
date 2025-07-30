#!/usr/bin/env bash
# ===========================================================================
#  header_sanity_check.sh — verify VM test headers compile cleanly
# ---------------------------------------------------------------------------
#  Iterates over tests/vm/*.c and ensures they compile with strict warnings
#  enabled. Paths containing spaces are correctly handled.
# ===========================================================================

set -euo pipefail

CC=${CC:-gcc}
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Include directories as an array to avoid word-splitting issues
INCLUDE_DIRS=("$REPO_ROOT/sys" "$REPO_ROOT/sys/vm/include")

# Build compiler arguments
INC_ARGS=()
for inc in "${INCLUDE_DIRS[@]}"; do
    INC_ARGS+=( -idirafter "$inc" )
done

for src in "$REPO_ROOT"/tests/vm/*.c; do
    [ -e "$src" ] || continue
    echo "Checking $src"
    "$CC" -Wall -Wextra -Werror -std=c17 -fsyntax-only "${INC_ARGS[@]}" "$src"
done

