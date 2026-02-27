#!/bin/bash
# XINIM QEMU Interactive Test Script
# Targets x86_64 v1 with Serial/Telnet interaction

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_BIN="${PROJECT_ROOT}/build/Debug/xinim"

if [[ ! -f "$KERNEL_BIN" ]]; then
    echo "Error: Kernel binary not found at $KERNEL_BIN"
    echo "Please build with: cmake --build build/Debug --target xinim"
    exit 1
fi

echo "Starting XINIM in QEMU (x86_64 v1)..."
echo "==========================================="
echo "Serial Console (COM1): Local Terminal"
echo "Telnet Console (COM2): localhost:4444"
echo "QEMU Monitor:          localhost:4445"
echo "==========================================="

# QEMU Optimized Configuration for Stability & Performance
# -cpu qemu64: Standard x86_64 v1
# -accel kvm: Use KVM if available
# -m 512: Stable memory baseline
# -serial stdio: Interactive shell on COM1
# -serial telnet:localhost:4444,server,nowait: Interactive shell on COM2 (mapped to shell() if implemented)
# -monitor telnet:localhost:4445,server,nowait: Programmatic monitor access

qemu-system-x86_64 \
    -cpu qemu64 \
    -m 512M \
    -kernel "$KERNEL_BIN" \
    -serial stdio \
    -serial telnet:localhost:4444,server,nowait \
    -monitor telnet:localhost:4445,server,nowait \
    -display none \
    -no-reboot \
    -enable-kvm 2>/dev/null || \
qemu-system-x86_64 \
    -cpu qemu64 \
    -m 512M \
    -kernel "$KERNEL_BIN" \
    -serial stdio \
    -serial telnet:localhost:4444,server,nowait \
    -monitor telnet:localhost:4445,server,nowait \
    -display none \
    -no-reboot
