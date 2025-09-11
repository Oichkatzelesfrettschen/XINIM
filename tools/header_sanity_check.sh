#!/usr/bin/env bash
#
# scripts/header_sanity_check.sh
# ------------------------------------------------------------------------------
# \file header_sanity_check.sh
# \brief Ensure VM test sources compile cleanly with strict flags and headers only.
#
# This script compiles each C source file in tests/vm/ with:
#  - strict warnings: -Wall -Wextra -Werror
#  - syntax-only check: -fsyntax-only
#  - language standard: -std=c17
#  - include paths: sys/ and sys/vm/include only
#
# It handles spaces in paths by constructing argument arrays and reports success
# or failure per file. Intended for CI and local sanity checks.
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

Ensures every C source in tests/vm/ compiles cleanly with strict flags
and only the configured include directories.

Options:
  --help        Show this message and exit.

Environment:
  CC            C compiler to use (default: gcc)
EOF
  exit 0
}

if [[ "${1:-}" == "--help" ]]; then
  print_usage
fi

# Compiler override
CC=${CC:-gcc}

# Determine repository root (assume script in scripts/)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

# Fail if no VM tests directory exists
if [[ ! -d "$REPO_ROOT/tests/vm" ]]; then
  echo "Error: tests/vm directory not found under $REPO_ROOT." >&2
  exit 1
fi

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

# Include directories (only these, to catch missing header deps)
INCLUDE_DIRS=(
  "$REPO_ROOT/sys"
  "$REPO_ROOT/sys/vm/include"
)

# Append include paths
for dir in "${INCLUDE_DIRS[@]}"; do
  ARGS+=( -idirafter "$dir" )
done

# Track if any sources were processed
found=0

# Iterate over VM test sources
for src in "$REPO_ROOT"/tests/vm/*.c; do
  [[ -e "$src" ]] || continue
  found=1
  echo -n "Checking syntax of $src... "
  if "$CC" "${ARGS[@]}" "$src"; then
    echo "OK"
  else
    echo "FAIL"
    exit 1
  fi
done

if [[ $found -eq 0 ]]; then
  echo "Error: No VM test sources found in $REPO_ROOT/tests/vm/." >&2
  exit 1
fi

echo "✔ All VM test headers compile cleanly."