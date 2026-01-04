#!/bin/bash
# Pre-commit hook for clang-format
# Formats all staged C/C++ files

set -e

# Find all staged C/C++ files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cc|cpp|cxx|h|hh|hpp)$' || true)

if [ -z "$STAGED_FILES" ]; then
    echo "No C/C++ files to format"
    exit 0
fi

echo "Running clang-format on staged files..."

# Format each file
for FILE in $STAGED_FILES; do
    if [ -f "$FILE" ]; then
        echo "  Formatting: $FILE"
        clang-format -i -style=file "$FILE"
        git add "$FILE"
    fi
done

echo "✓ clang-format complete"
exit 0
