#!/usr/bin/env bash
#
# scripts/header_sanity_check.sh
# ------------------------------------------------------------------------------
# \file header_sanity_check.sh
# \brief Compile & sanity‐check VM test headers under strict flags.
#
# Verifies that all C sources in tests/vm/ compile cleanly with:
#  - strict warnings: -Wall -Wextra -Werror
#  - language: -std=c17
#  - syntax‐only: -fsyntax-only
#  - correct include paths (sys/, sys/vm/include)
#
# Handles spaces in repository paths by building argument arrays.
#
# Usage:
#   header_sanity_check.sh [--help] [CC=<compiler>]
#
# Options:
#   --help      Show this help message and exit.
#
# Environment:
#   CC          C compiler to use (default: gcc)
#
set -euo pipefail
[ -n "${VERBOSE:-}" ] && set -x
trap 'echo "header_sanity_check.sh: error on line ${LINENO}: ${BASH_COMMAND}" >&2' ERR

print_usage() {
  cat <<EOF
Usage: $(basename "$0") [--help] [CC=<compiler>]

Verifies that each VM test source under tests/vm/ compiles cleanly
with strict warnings and the proper include paths.

Options:
  --help        Show this message and exit.

Environment:
  CC            C compiler to use (default: gcc)
EOF
  exit 0
}

# Handle help flag
if [[ "${1:-}" == "--help" ]]; then
  print_usage
fi

# Compiler (override via environment if desired)
CC=${CC:-gcc}

# Determine repository root (script resides in scripts/)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

# Enable nullglob so the loop skips if no files match
shopt -s nullglob

# Base compiler arguments
ARGS=(
  -fsyntax-only
  -Wall
  -Wextra
  -Werror
  -std=c17
)

# Include directories
INCLUDE_DIRS=(
  "$REPO_ROOT/sys"
  "$REPO_ROOT/sys/vm/include"
)

# Append include paths using -idirafter to prefer system headers
for dir in "${INCLUDE_DIRS[@]}"; do
  ARGS+=( -idirafter "$dir" )
done

# Track if any VM tests were found
found=0

# Iterate and check each VM test source
for src in "$REPO_ROOT"/tests/vm/*.c; do
  found=1
  echo "Checking syntax of $src"
  "$CC" "${ARGS[@]}" "$src"
done

if [[ $found -eq 0 ]]; then
  echo "No VM test sources found in $REPO_ROOT/tests/vm/." >&2
  exit 1
fi

echo "✔ All VM test headers compile cleanly."