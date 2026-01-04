#!/bin/bash
# Run cppcheck on source files

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

echo "Running cppcheck on kernel and core components..."

# Run cppcheck on specific directories to avoid segfault
cppcheck \
    --enable=warning,style,performance,portability \
    --std=c++23 \
    --quiet \
    --template='{file}:{line}: {severity}: {message}' \
    src/kernel/*.cpp \
    src/mm/*.cpp \
    src/hal/*.cpp \
    2>&1 | tee docs/analysis/reports/static_analysis/cppcheck_latest.txt

echo "✓ cppcheck complete"
echo "Report: docs/analysis/reports/static_analysis/cppcheck_latest.txt"
exit 0
