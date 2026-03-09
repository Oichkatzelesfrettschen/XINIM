#!/bin/sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

if [ -n "${XINIM_BUILD_ROOT:-}" ] && [ -x "${XINIM_BUILD_ROOT}/xash" ]; then
    XASH_BIN="${XINIM_BUILD_ROOT}/xash"
elif [ -x "${PROJECT_ROOT}/build/x86_64/Debug/xash" ]; then
    XASH_BIN="${PROJECT_ROOT}/build/x86_64/Debug/xash"
else
    echo "SKIP: hosted xash binary not built"
    exit 77
fi

OUTPUT="$(printf 'help\nexit\n' | "${XASH_BIN}")"

printf '%s\n' "${OUTPUT}" | grep -q "XASH Shell" || {
    echo "FAIL: xash banner missing"
    exit 1
}

printf '%s\n' "${OUTPUT}" | grep -q "Built-in commands" || {
    echo "FAIL: help output missing built-ins section"
    exit 1
}

printf '%s\n' "${OUTPUT}" | grep -q "fg \[%n\]" || {
    echo "FAIL: help output missing job-control built-ins"
    exit 1
}

echo "PASS: hosted xash smoke test"
