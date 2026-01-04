#!/bin/bash
# Run clang-tidy on changed C/C++ files

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

# Check if compile_commands.json exists
if [ ! -f "build/compile_commands.json" ]; then
    echo "Warning: compile_commands.json not found. Run cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    echo "Skipping clang-tidy..."
    exit 0
fi

# Find staged C/C++ files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cc|cpp|cxx)$' || true)

if [ -z "$STAGED_FILES" ]; then
    echo "No C/C++ source files to check"
    exit 0
fi

echo "Running clang-tidy on staged files..."

FAILED=0
for FILE in $STAGED_FILES; do
    if [ -f "$FILE" ]; then
        echo "  Checking: $FILE"
        if clang-tidy -p build "$FILE" 2>&1 | grep -E "warning:|error:"; then
            FAILED=1
        fi
    fi
done

if [ $FAILED -eq 1 ]; then
    echo "✗ clang-tidy found issues"
    exit 1
fi

echo "✓ clang-tidy complete"
exit 0
