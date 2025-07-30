#!/usr/bin/env bash
#
# scripts/header_sanity_check.sh
# ------------------------------------------------------------------------------
# Verifies that VM test sources under tests/vm/*.c compile cleanly with strict
# warnings and correct include paths. Handles repository paths with spaces by
# building compiler arguments as arrays.
# ------------------------------------------------------------------------------
set -euo pipefail
[ -n "${VERBOSE:-}" ] && set -x
trap 'echo "header_sanity_check.sh: error on line ${LINENO}: ${BASH_COMMAND}" >&2' ERR

print_usage() {
    cat <<EOF
Usage: $(basename "$0") [--help] [CC=<compiler>]

Options:
  --help        Show this message and exit.

Environment:
  CC            C compiler to use (default: gcc)

Description:
  Ensures each C source in tests/vm/ compiles with -fsyntax-only using
  strict flags (-Wall -Wextra -Werror) and the proper include paths:
    sys/ and sys/vm/include under the repository root.
EOF
    exit 0
}

if [[ "${1:-}" == "--help" ]]; then
    print_usage
fi

# C compiler (override via environment if desired)
CC=${CC:-gcc}

# Determine repository root (this script lives in scripts/)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

# Include directories
INCLUDE_DIRS=(
    "$REPO_ROOT/sys"
    "$REPO_ROOT/sys/vm/include"
)

# Base compiler arguments
ARGS=(
    -fsyntax-only
    -Wall
    -Wextra
    -Werror
    -std=c17
)

# Append include paths using -idirafter to prefer system headers
for dir in "${INCLUDE_DIRS[@]}"; do
    ARGS+=( -idirafter "$dir" )
done

# Track if any test files were found
found=0

# Iterate over VM test sources
for src in "$REPO_ROOT"/tests/vm/*.c; do
    [[ -e "$src" ]] || continue
    found=1
    echo "Checking syntax of $src"
    "$CC" "${ARGS[@]}" "$src"
done

if [[ $found -eq 0 ]]; then
    echo "No VM test sources found in $REPO_ROOT/tests/vm/." >&2
    exit 1
fi

echo "✔ All VM test headers compile cleanly."