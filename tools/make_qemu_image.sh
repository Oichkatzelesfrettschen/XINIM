#!/usr/bin/env bash
#
# make_qemu_image.sh - Create bootable XINIM disk images for QEMU
#
# This script creates disk images for testing XINIM in QEMU on both
# x86_64 and ARM64 architectures. It supports multiple boot protocols
# including Limine, GRUB, and direct kernel boot.
#
# Usage:
#    ./tools/make_qemu_image.sh [options]
#
# Options:
#    --arch=<x86_64|arm64>  Target architecture (default: auto-detect)
#    --size=<size>          Disk size (default: 1G)
#    --boot=<limine|grub|direct> Boot method (default: limine)
#    --kernel=<path>        Path to kernel binary
#    --output=<path>        Output image path (default: xinim-<arch>.img)

set -euo pipefail

# Default settings
ARCH=""
DISK_SIZE="1G"
BOOT_METHOD="limine"
KERNEL_PATH=""
OUTPUT_PATH=""
BUILD_DIR="build"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

show_help() {
    grep '^#' "$0" | head -20 | tail -n +2 | sed -E 's/^# ?//'
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch=*)
            ARCH="${1#*=}"
            ;;
        --size=*)
            DISK_SIZE="${1#*=}"
            ;;
        --boot=*)
            BOOT_METHOD="${1#*=}"
            ;;
        --kernel=*)
            KERNEL_PATH="${1#*=}"
            ;;
        --output=*)
            OUTPUT_PATH="${1#*=}"
            ;;
        --help|-h)
            show_help
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            ;;
    esac
    shift
done

# Auto-detect architecture
if [[ -z "$ARCH" ]]; then
    if [[ "$(uname -m)" == "arm64" ]] || [[ "$(uname -m)" == "aarch64" ]]; then
        ARCH="arm64"
    else
        ARCH="x86_64"
    fi
    log_info "Auto-detected architecture: $ARCH"
fi

# Set default output path
if [[ -z "$OUTPUT_PATH" ]]; then
    OUTPUT_PATH="xinim-${ARCH}.img"
fi

# Find kernel if not specified
if [[ -z "$KERNEL_PATH" ]]; then
    # Look for kernel in common locations
    KERNEL_SEARCH_PATHS=(
        "${BUILD_DIR}/kernel/xinim.elf"
        "${BUILD_DIR}/xinim-kernel"
        "${BUILD_DIR}/xinim"
        "kernel/xinim.elf"
    )
    
    for path in "${KERNEL_SEARCH_PATHS[@]}"; do
        if [[ -f "$path" ]]; then
            KERNEL_PATH="$path"
            log_info "Found kernel: $KERNEL_PATH"
            break
        fi
    done
    
    if [[ -z "$KERNEL_PATH" ]]; then
        log_error "Kernel not found. Please build first or specify with --kernel"
        exit 1
    fi
fi

# Validate kernel exists
if [[ ! -f "$KERNEL_PATH" ]]; then
    log_error "Kernel not found: $KERNEL_PATH"
    exit 1
fi

# Create disk image
create_disk_image() {
    log_info "Creating disk image: $OUTPUT_PATH (size: $DISK_SIZE)"
    
    # Create sparse disk image
    dd if=/dev/zero of="$OUTPUT_PATH" bs=1 count=0 seek="$DISK_SIZE" 2>/dev/null || \
        truncate -s "$DISK_SIZE" "$OUTPUT_PATH"
    
    # Create partition table based on architecture
    if [[ "$ARCH" == "x86_64" ]]; then
        # Create GPT partition table for x86_64
        parted -s "$OUTPUT_PATH" \
            mklabel gpt \
            mkpart ESP fat32 1MiB 512MiB \
            set 1 esp on \
            mkpart primary ext4 512MiB 100%
    else
        # Create GPT partition table for ARM64
        parted -s "$OUTPUT_PATH" \
            mklabel gpt \
            mkpart ESP fat32 1MiB 512MiB \
            set 1 esp on \
            mkpart primary ext4 512MiB 100%
    fi
    
    log_success "Disk image created"
}

# Setup loop device and mount partitions
setup_loop_device() {
    log_info "Setting up loop device..."
    
    # Find free loop device
    LOOP_DEV=$(sudo losetup -f)
    
    # Attach image to loop device
    sudo losetup -P "$LOOP_DEV" "$OUTPUT_PATH"
    
    # Create mount points
    MOUNT_BOOT=$(mktemp -d /tmp/xinim-boot.XXXXXX)
    MOUNT_ROOT=$(mktemp -d /tmp/xinim-root.XXXXXX)
    
    # Format partitions
    sudo mkfs.fat -F32 "${LOOP_DEV}p1"
    sudo mkfs.ext4 -q "${LOOP_DEV}p2"
    
    # Mount partitions
    sudo mount "${LOOP_DEV}p1" "$MOUNT_BOOT"
    sudo mount "${LOOP_DEV}p2" "$MOUNT_ROOT"
    
    log_success "Loop device configured: $LOOP_DEV"
}

# Install Limine bootloader
install_limine() {
    log_info "Installing Limine bootloader..."
    
    # Check if Limine is available
    if [[ ! -d "third_party/limine-src" ]]; then
        log_warning "Limine not found, cloning..."
        git clone https://github.com/limine-bootloader/limine.git third_party/limine-src
        (cd third_party/limine-src && make)
    fi
    
    # Copy Limine files
    sudo cp third_party/limine-src/limine-uefi-cd.bin "$MOUNT_BOOT/limine-cd.bin"
    sudo cp third_party/limine-src/limine-uefi-cd-efi.bin "$MOUNT_BOOT/limine-cd-efi.bin"
    
    # Create Limine configuration
    sudo tee "$MOUNT_BOOT/limine.cfg" > /dev/null << EOF
TIMEOUT=5

:XINIM
PROTOCOL=limine
KERNEL_PATH=boot:///xinim.elf
CMDLINE=console=ttyS0,115200 tick=auto
EOF
    
    # Copy kernel
    sudo cp "$KERNEL_PATH" "$MOUNT_BOOT/xinim.elf"
    
    # Install Limine to disk
    if [[ "$ARCH" == "x86_64" ]]; then
        sudo third_party/limine-src/limine bios-install "$LOOP_DEV"
    fi
    
    log_success "Limine installed"
}

# Install GRUB bootloader
install_grub() {
    log_info "Installing GRUB bootloader..."
    
    local grub_arch=""
    case "$ARCH" in
        x86_64)
            grub_arch="x86_64-efi"
            ;;
        arm64|aarch64)
            grub_arch="arm64-efi"
            ;;
    esac
    
    # Install GRUB
    sudo grub-install \
        --target="$grub_arch" \
        --efi-directory="$MOUNT_BOOT" \
        --boot-directory="$MOUNT_ROOT/boot" \
        --removable \
        --recheck \
        "$LOOP_DEV"
    
    # Create GRUB configuration
    sudo tee "$MOUNT_ROOT/boot/grub/grub.cfg" > /dev/null << EOF
set timeout=5
set default=0

menuentry "XINIM" {
    multiboot2 /boot/xinim.elf console=ttyS0,115200
    boot
}
EOF
    
    # Copy kernel
    sudo cp "$KERNEL_PATH" "$MOUNT_ROOT/boot/xinim.elf"
    
    log_success "GRUB installed"
}

# Setup direct kernel boot (for testing)
setup_direct_boot() {
    log_info "Setting up direct kernel boot..."
    
    # Copy kernel to root
    sudo cp "$KERNEL_PATH" "$MOUNT_ROOT/xinim.elf"
    
    # Create minimal init script
    sudo tee "$MOUNT_ROOT/init" > /dev/null << 'EOF'
#!/bin/sh
echo "XINIM Direct Boot"
exec /xinim.elf
EOF
    sudo chmod +x "$MOUNT_ROOT/init"
    
    log_success "Direct boot configured"
}

# Cleanup function
cleanup() {
    if [[ -n "${MOUNT_BOOT:-}" ]] && mountpoint -q "$MOUNT_BOOT" 2>/dev/null; then
        sudo umount "$MOUNT_BOOT"
        rmdir "$MOUNT_BOOT"
    fi
    
    if [[ -n "${MOUNT_ROOT:-}" ]] && mountpoint -q "$MOUNT_ROOT" 2>/dev/null; then
        sudo umount "$MOUNT_ROOT"
        rmdir "$MOUNT_ROOT"
    fi
    
    if [[ -n "${LOOP_DEV:-}" ]]; then
        sudo losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi
}

# Set trap for cleanup
trap cleanup EXIT

# Main execution
main() {
    log_info "XINIM QEMU Image Builder"
    log_info "========================"
    log_info "Architecture: $ARCH"
    log_info "Boot method: $BOOT_METHOD"
    log_info "Kernel: $KERNEL_PATH"
    log_info "Output: $OUTPUT_PATH"
    
    # Check for required tools
    for tool in parted mkfs.fat mkfs.ext4 losetup; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            log_error "Required tool not found: $tool"
            exit 1
        fi
    done
    
    # Create disk image
    create_disk_image
    
    # Setup loop device
    setup_loop_device
    
    # Install bootloader
    case "$BOOT_METHOD" in
        limine)
            install_limine
            ;;
        grub)
            install_grub
            ;;
        direct)
            setup_direct_boot
            ;;
        *)
            log_error "Unknown boot method: $BOOT_METHOD"
            exit 1
            ;;
    esac
    
    # Sync and cleanup
    sync
    cleanup
    
    # Make image readable
    chmod 644 "$OUTPUT_PATH"
    
    log_success "Image created successfully: $OUTPUT_PATH"
    log_info "To test the image, run:"
    echo "    ./tools/run_qemu.sh --arch=$ARCH $OUTPUT_PATH"
}

# Check if running as root (some operations need it)
if [[ "$BOOT_METHOD" != "direct" ]] && [[ $EUID -ne 0 ]] && ! command -v sudo >/dev/null 2>&1; then
    log_error "This script requires sudo for disk operations"
    exit 1
fi

# Run main
main