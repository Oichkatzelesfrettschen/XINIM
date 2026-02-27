#!/bin/bash
# Script to run Xinim in a Pentium i386 QEMU environment

# Configuration
DISK_IMG="xinim_disk.img"
DISK_SIZE="2G"
MEMORY="256M"
CPU_MODEL="pentium3"

if ! command -v qemu-system-i386 &> /dev/null; then
    echo "qemu-system-i386 not found"
    echo "Install with: sudo pacman -S qemu-system-x86 (Arch)"
    echo "Or: sudo apt-get install qemu-system-x86 (Debian/Ubuntu)"
    exit 1
fi

# Create disk image if it doesn't exist
if [ ! -f "$DISK_IMG" ]; then
    echo "Creating $DISK_SIZE disk image: $DISK_IMG..."
    qemu-img create -f raw "$DISK_IMG" "$DISK_SIZE"
fi

echo "Starting QEMU (i386 Pentium)..."
echo "Press Ctrl+A, X to exit."

# Note: We use -serial stdio to capture kernel output if configured to write to COM1
qemu-system-i386 \
    -M pc \
    -cpu "$CPU_MODEL" \
    -m "$MEMORY" \
    -drive file="$DISK_IMG",format=raw,index=0,media=disk \
    -netdev user,id=net0 -device pcnet,netdev=net0 \
    -vga cirrus \
    -serial stdio \
    -boot menu=on \
    "$@"
