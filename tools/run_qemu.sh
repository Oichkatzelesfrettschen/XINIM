#!/usr/bin/env bash
## \file run_qemu.sh
## \brief Launch XINIM disk image under QEMU for x86_64 or ARM64.
##
## Usage: ./tools/run_qemu.sh [--arch=<x86_64|arm64>] <disk_image> [extra_qemu_args...]
## This script boots the specified disk image using the appropriate QEMU
## architecture with optimized settings for each platform.
##
## Features:
## - Auto-detects host architecture if not specified
## - Hardware acceleration (KVM/HVF) when available
## - UEFI boot support for both architectures
## - Serial console output
## - Network support

set -euo pipefail

# Default settings
ARCH=""
IMAGE=""
MEMORY="2G"
CORES="2"
ENABLE_NET=true
ENABLE_ACCEL=true
EXTRA_ARGS=()

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $*"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch=*)
            ARCH="${1#*=}"
            ;;
        --memory=*|--mem=*)
            MEMORY="${1#*=}"
            ;;
        --cores=*)
            CORES="${1#*=}"
            ;;
        --no-net)
            ENABLE_NET=false
            ;;
        --no-accel)
            ENABLE_ACCEL=false
            ;;
        --help|-h)
            cat << EOF
Usage: $0 [options] <disk_image> [extra_qemu_args...]

Options:
    --arch=<x86_64|arm64>  Target architecture (auto-detected if not specified)
    --memory=<size>        RAM size (default: 2G)
    --cores=<n>           CPU cores (default: 2)
    --no-net              Disable networking
    --no-accel            Disable hardware acceleration
    --help                Show this help message

Examples:
    $0 xinim.img                        # Auto-detect architecture
    $0 --arch=x86_64 xinim.img         # Force x86_64
    $0 --arch=arm64 --memory=4G xinim.img  # ARM64 with 4GB RAM

EOF
            exit 0
            ;;
        -*)
            # Unknown option, pass to QEMU
            EXTRA_ARGS+=("$1")
            ;;
        *)
            # First non-option argument is the disk image
            if [[ -z "$IMAGE" ]]; then
                IMAGE="$1"
            else
                EXTRA_ARGS+=("$1")
            fi
            ;;
    esac
    shift
done

# Validate disk image
if [[ -z "$IMAGE" ]]; then
    log_error "No disk image specified"
    echo "Usage: $0 [options] <disk_image> [extra_qemu_args...]" >&2
    exit 1
fi

if [[ ! -f "$IMAGE" ]]; then
    log_error "Disk image not found: $IMAGE"
    exit 1
fi

# Auto-detect architecture if not specified
if [[ -z "$ARCH" ]]; then
    if [[ "$(uname -m)" == "arm64" ]] || [[ "$(uname -m)" == "aarch64" ]]; then
        ARCH="arm64"
    else
        ARCH="x86_64"
    fi
    log_info "Auto-detected architecture: $ARCH"
else
    log_info "Using specified architecture: $ARCH"
fi

# Function to check for acceleration support
check_acceleration() {
    local arch="$1"
    
    if [[ "$ENABLE_ACCEL" == false ]]; then
        echo "none"
        return
    fi
    
    case "$arch" in
        x86_64)
            # Check for KVM on Linux
            if [[ -e /dev/kvm ]] && [[ -r /dev/kvm ]] && [[ -w /dev/kvm ]]; then
                echo "kvm"
            # Check for HVF on macOS
            elif [[ "$(uname)" == "Darwin" ]] && [[ "$(uname -m)" == "x86_64" ]]; then
                echo "hvf"
            # Check for WHPX on Windows
            elif [[ -n "${WINDIR:-}" ]]; then
                echo "whpx"
            else
                echo "tcg"
            fi
            ;;
        arm64)
            # Check for HVF on macOS Apple Silicon
            if [[ "$(uname)" == "Darwin" ]] && [[ "$(uname -m)" == "arm64" ]]; then
                echo "hvf"
            # Check for KVM on Linux ARM64
            elif [[ -e /dev/kvm ]] && [[ "$(uname -m)" == "aarch64" ]]; then
                echo "kvm"
            else
                echo "tcg"
            fi
            ;;
    esac
}

# Launch QEMU based on architecture
case "$ARCH" in
    x86_64)
        log_info "Launching XINIM for x86_64..."
        
        # Detect acceleration
        ACCEL=$(check_acceleration x86_64)
        log_info "Using acceleration: $ACCEL"
        
        # Build QEMU command
        QEMU_ARGS=(
            -m "$MEMORY"
            -smp "$CORES"
            -serial mon:stdio
        )
        
        # Add acceleration
        if [[ "$ACCEL" != "none" ]]; then
            QEMU_ARGS+=(-accel "$ACCEL")
            if [[ "$ACCEL" == "kvm" ]]; then
                QEMU_ARGS+=(-cpu host)
            else
                QEMU_ARGS+=(-cpu max)
            fi
        else
            QEMU_ARGS+=(-cpu max)
        fi
        
        # Storage configuration
        QEMU_ARGS+=(
            -drive "file=${IMAGE},if=none,format=raw,id=hd0"
            -device ich9-ahci,id=ahci
            -device ide-hd,drive=hd0,bus=ahci.0
        )
        
        # UEFI firmware if available
        if [[ -n "${OVMF_CODE:-}" ]] && [[ -f "$OVMF_CODE" ]]; then
            log_info "Using UEFI firmware: $OVMF_CODE"
            QEMU_ARGS+=(-bios "$OVMF_CODE")
        elif [[ -f /usr/share/OVMF/OVMF_CODE.fd ]]; then
            log_info "Using system UEFI firmware"
            QEMU_ARGS+=(-bios /usr/share/OVMF/OVMF_CODE.fd)
        fi
        
        # Network configuration
        if [[ "$ENABLE_NET" == true ]]; then
            QEMU_ARGS+=(
                -netdev user,id=net0,hostfwd=tcp::2222-:22
                -device e1000,netdev=net0
            )
        fi
        
        # Add extra arguments
        QEMU_ARGS+=("${EXTRA_ARGS[@]}")
        
        # Launch QEMU
        log_success "Starting QEMU x86_64..."
        exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
        ;;
        
    arm64|aarch64)
        log_info "Launching XINIM for ARM64..."
        
        # Detect acceleration
        ACCEL=$(check_acceleration arm64)
        log_info "Using acceleration: $ACCEL"
        
        # Build QEMU command for ARM64
        QEMU_ARGS=(
            -M virt
            -m "$MEMORY"
            -smp "$CORES"
            -serial mon:stdio
            -nographic
        )
        
        # Add acceleration
        if [[ "$ACCEL" != "none" ]]; then
            QEMU_ARGS+=(-accel "$ACCEL")
            if [[ "$ACCEL" == "hvf" ]] || [[ "$ACCEL" == "kvm" ]]; then
                QEMU_ARGS+=(-cpu host)
            else
                QEMU_ARGS+=(-cpu cortex-a72)
            fi
        else
            QEMU_ARGS+=(-cpu cortex-a72)
        fi
        
        # Storage configuration for ARM64
        QEMU_ARGS+=(
            -drive "file=${IMAGE},if=none,format=raw,id=hd0"
            -device virtio-blk-pci,drive=hd0
        )
        
        # UEFI firmware for ARM64
        if [[ -n "${AAVMF_CODE:-}" ]] && [[ -f "$AAVMF_CODE" ]]; then
            log_info "Using ARM64 UEFI firmware: $AAVMF_CODE"
            QEMU_ARGS+=(-bios "$AAVMF_CODE")
        elif [[ -f /usr/share/AAVMF/AAVMF_CODE.fd ]]; then
            log_info "Using system ARM64 UEFI firmware"
            QEMU_ARGS+=(-bios /usr/share/AAVMF/AAVMF_CODE.fd)
        elif [[ -f /opt/homebrew/share/qemu/edk2-aarch64-code.fd ]]; then
            log_info "Using Homebrew ARM64 UEFI firmware"
            QEMU_ARGS+=(-bios /opt/homebrew/share/qemu/edk2-aarch64-code.fd)
        fi
        
        # Network configuration
        if [[ "$ENABLE_NET" == true ]]; then
            QEMU_ARGS+=(
                -netdev user,id=net0,hostfwd=tcp::2222-:22
                -device virtio-net-pci,netdev=net0
            )
        fi
        
        # Add extra arguments
        QEMU_ARGS+=("${EXTRA_ARGS[@]}")
        
        # Launch QEMU
        log_success "Starting QEMU ARM64..."
        exec qemu-system-aarch64 "${QEMU_ARGS[@]}"
        ;;
        
    *)
        log_error "Unsupported architecture: $ARCH"
        log_error "Supported architectures: x86_64, arm64"
        exit 1
        ;;
esac
