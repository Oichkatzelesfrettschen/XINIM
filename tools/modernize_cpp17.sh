#!/bin/sh
# modernize_cpp17.sh - Prepare the codebase for C++17 modernization.
# This script renames source files to .cpp/.hpp, updates include statements,
# and inserts a temporary modernization header at the top of each file.
# It does not overwrite existing headers if they already contain the marker.

set -e

# Optional environment variable to limit processed files. 0 means no limit.
FILE_LIMIT="${FILE_LIMIT:-0}"

# Arrays of files renamed this pass
changed_headers=""
changed_sources=""
changed_asm=""

MOD_HEADER_C='/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/'

MOD_HEADER_ASM='; <<< WORK-IN-PROGRESS MODERNIZATION HEADER
; This assembly file will be updated for modern nasm syntax
; on arm64/x86_64.
; >>>'

# Rename .h to .hpp
count=0
for f in $(git ls-files '*.h' | grep -vE '^tools/c86/|^tools/minix/'); do
    if [ "$FILE_LIMIT" -gt 0 ] && [ "$count" -ge "$FILE_LIMIT" ]; then
        break
    fi
    git mv "$f" "${f%.h}.hpp"
    echo "Renamed $f to ${f%.h}.hpp"
    changed_headers="$changed_headers $f"
    count=$((count + 1))
done

# Rename .cpp to .cpppp
count=0
for f in $(git ls-files '*.cpp' | grep -vE '^tools/c86/|^tools/minix/'); do
    if [ "$FILE_LIMIT" -gt 0 ] && [ "$count" -ge "$FILE_LIMIT" ]; then
        break
    fi
    git mv "$f" "${f%.cpp}.cpppp"
    echo "Renamed $f to ${f%.cpp}.cpppp"
    changed_sources="$changed_sources $f"
    count=$((count + 1))
done

# Update include references
for h in $changed_headers; do
    base=$(basename "$h")
    newbase="${base%.h}.hpp"
    grep -rl --exclude-dir=.git "\"$base\"" . | \
        xargs -r sed -i "s|\"$base\"|\"$newbase\"|g"
done

# Insert header into .cpppp and .hpp files
for src in $changed_sources; do
    f="${src%.cpp}.cpppp"
    if [ -f "$f" ] && ! grep -q 'WORK-IN-PROGRESS MODERNIZATION HEADER' "$f"; then
        tmpfile=$(mktemp)
        printf '%s\n\n' "$MOD_HEADER_C" > "$tmpfile"
        cat "$f" >> "$tmpfile"
        mv "$tmpfile" "$f"
    fi
done

for hdr in $changed_headers; do
    f="${hdr%.h}.hpp"
    if [ -f "$f" ] && ! grep -q 'WORK-IN-PROGRESS MODERNIZATION HEADER' "$f"; then
        tmpfile=$(mktemp)
        printf '%s\n\n' "$MOD_HEADER_C" > "$tmpfile"
        cat "$f" >> "$tmpfile"
        mv "$tmpfile" "$f"
    fi
done

# Rename .s or .asm to .nasm and insert header
count=0
for f in $(git ls-files '*.s' '*.asm' '*.S' | grep -vE '^tools/c86/|^tools/minix/'); do
    if [ "$FILE_LIMIT" -gt 0 ] && [ "$count" -ge "$FILE_LIMIT" ]; then
        break
    fi
    newf="${f%.*}.nasm"
    git mv "$f" "$newf"
    echo "Renamed $f to $newf"
    changed_asm="$changed_asm $newf"
    count=$((count + 1))
done

for f in $changed_asm; do
    if [ -f "$f" ] && ! grep -q 'WORK-IN-PROGRESS MODERNIZATION HEADER' "$f"; then
        tmpfile=$(mktemp)
        printf '%s\n\n' "$MOD_HEADER_ASM" > "$tmpfile"
        cat "$f" >> "$tmpfile"
        mv "$tmpfile" "$f"
    fi
done

printf '\nModernization pass complete. Review changes and build.\n'
