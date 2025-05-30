#!/bin/sh
# modernize_cpp23.sh - Prepare the codebase for C++23 modernization.
# This script renames source files to .cpp/.hpp, updates include statements,
# and inserts a temporary modernization header at the top of each file.
# It does not overwrite existing headers if they already contain the marker.

set -e

MOD_HEADER_C="/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER\n  This repository is a work in progress to reproduce the\n  original MINIX simplicity on modern 32-bit and 64-bit\n  ARM and x86/x86_64 hardware using C++23.\n>>>*/"

MOD_HEADER_ASM="; <<< WORK-IN-PROGRESS MODERNIZATION HEADER\n; This assembly file will be updated for modern nasm syntax\n; on arm64/x86_64.\n; >>>"

# Rename .h to .hpp
for f in $(git ls-files '*.h'); do
    git mv "$f" "${f%.h}.hpp"
    echo "Renamed $f to ${f%.h}.hpp"
done

# Rename .c to .cpp
for f in $(git ls-files '*.c'); do
    git mv "$f" "${f%.c}.cpp"
    echo "Renamed $f to ${f%.c}.cpp"
done

# Update include references
grep -rl --exclude-dir=.git -e '\.h"' . | \
    xargs sed -i 's/\(#include[[:space:]]*"[^"]*\)\.h"/\1.hpp"/g'

# Insert header into .cpp and .hpp files
for f in $(git ls-files '*.cpp' '*.hpp'); do
    if ! grep -q 'WORK-IN-PROGRESS MODERNIZATION HEADER' "$f"; then
        tmpfile=$(mktemp)
        printf '%s\n\n' "$MOD_HEADER_C" > "$tmpfile"
        cat "$f" >> "$tmpfile"
        mv "$tmpfile" "$f"
    fi
done

# Insert header into assembly files
for f in $(git ls-files '*.s' '*.S'); do
    if ! grep -q 'WORK-IN-PROGRESS MODERNIZATION HEADER' "$f"; then
        tmpfile=$(mktemp)
        printf '%s\n\n' "$MOD_HEADER_ASM" > "$tmpfile"
        cat "$f" >> "$tmpfile"
        mv "$tmpfile" "$f"
    fi
done

printf '\nModernization pass complete. Review changes and build.\n'
