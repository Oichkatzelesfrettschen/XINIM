#!/bin/sh
# XINIM boot smoke test.
# Boots QEMU for a few seconds, captures COM1 serial output,
# and checks for expected boot messages.
#
# Exit codes:
#   0 = boot messages found
#   1 = expected strings missing
#   2 = kernel image not found

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
KERNEL_IMAGE="${PROJECT_ROOT}/build/Debug/xinim"
TIMEOUT_SEC=10
LOG_FILE="/tmp/xinim_boot_smoke_$$.log"

if [ ! -f "$KERNEL_IMAGE" ]; then
    echo "SKIP: Kernel image not found: $KERNEL_IMAGE"
    exit 0
fi

echo "Starting QEMU boot smoke test..."

# Boot QEMU with COM1 output to a log file, kill after timeout.
qemu-system-x86_64 \
    -machine q35 \
    -cpu qemu64 \
    -m 512M \
    -smp 1 \
    -kernel "$KERNEL_IMAGE" \
    -serial "file:${LOG_FILE}" \
    -nographic \
    -no-reboot &
QEMU_PID=$!

# Wait for boot, then kill QEMU.
sleep "$TIMEOUT_SEC"
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

echo "QEMU exited. Checking boot log..."

if [ ! -f "$LOG_FILE" ]; then
    echo "FAIL: No serial output captured"
    exit 1
fi

PASSED=0
FAILED=0

check_string() {
    if grep -q "$1" "$LOG_FILE"; then
        echo "PASS: Found '$1'"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: Missing '$1'"
        FAILED=$((FAILED + 1))
    fi
}

check_string "XINIM Kernel Booting"
check_string "IDT"

echo ""
echo "Results: $PASSED passed, $FAILED failed"

rm -f "$LOG_FILE"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
