#!/bin/bash
# XINIM QEMU Launch Script for x86_64
# Optimized settings for running XINIM kernel in x86_64 QEMU session

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() { echo -e "${BLUE}[QEMU]${NC} $1"; }
print_success() { echo -e "${GREEN}[QEMU]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }

# Default configuration for x86_64
BUILD_ROOT="${XINIM_BUILD_ROOT:-${PROJECT_ROOT}/build/x86_64/Debug}"
IMAGE_ROOT="${XINIM_IMAGE_ROOT:-${BUILD_ROOT}/images}"
BOOT_IMAGE="${XINIM_QEMU_BOOT_IMAGE:-${IMAGE_ROOT}/x86_64/xinim-x86_64.iso}"
MEMORY="512M"
CPU_TYPE="qemu64"
MACHINE="pc-q35-11.1"
ACCEL_ARGS=()
SERIAL_OUTPUT="stdio"
KSHELL_PORT=4555
DISPLAY="-nographic"
DEBUG_MODE=false
GDB_PORT=1234
KERNEL_CMDLINE=""
SMP_CPUS="1"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --boot-image)
            BOOT_IMAGE="$2"
            shift 2
            ;;
        -m|--memory)
            MEMORY="$2"
            shift 2
            ;;
        --cpu)
            CPU_TYPE="$2"
            shift 2
            ;;
        --machine)
            MACHINE="$2"
            shift 2
            ;;
        --smp)
            SMP_CPUS="$2"
            shift 2
            ;;
        -g|--debug)
            DEBUG_MODE=true
            shift
            ;;
        --gdb-port)
            GDB_PORT="$2"
            shift 2
            ;;
        --display)
            DISPLAY=""
            shift
            ;;
        --kshell-port)
            KSHELL_PORT="$2"
            shift 2
            ;;
        --cmdline)
            KERNEL_CMDLINE="$2"
            shift 2
            ;;
        -h|--help)
            cat << EOF
Usage: $0 [OPTIONS]

XINIM x86_64 QEMU Launch Script

Options:
  --boot-image PATH       Bootable disk/ISO image for QEMU (recommended)
  -m, --memory SIZE       Memory size (default: 512M)
  --cpu TYPE             CPU type (default: qemu64)
                         Options: qemu64, host, Nehalem, SandyBridge, IvyBridge,
                                  Haswell, Broadwell, Skylake-Client, Cascadelake-Server
  --machine TYPE         Machine type (required: pc-q35-11.1)
  --smp N                Number of CPUs (default: 1)
  -g, --debug            Enable GDB debugging
  --gdb-port PORT        GDB server port (default: 1234)
  --display              Enable graphical display (default: serial only)
  --kshell-port PORT     TCP port for kshell on COM2 (default: 4555)
  --cmdline "ARGS"       Kernel command line arguments
  -h, --help             Show this help message

Examples:
  # Boot from a prebuilt image
  $0 --boot-image "\$XINIM_IMAGE_ROOT/x86_64/xinim-x86_64.iso"

  # Boot with more memory and CPUs
  $0 -m 2G --smp 4

  # Boot with modern Skylake CPU features
  $0 --cpu Skylake-Client

  # Debug mode with GDB
  $0 -g

  # Boot with host CPU passthrough (requires KVM)
  $0 --cpu host

  # Connect to kshell via COM2 (after boot)
  # socat - TCP:localhost:4555

Recommended CPU types for x86_64:
  - qemu64:           Generic 64-bit x86 (best compatibility)
  - host:             Pass through host CPU features (requires KVM)
  - Nehalem:          Intel Core i7 (2008) - SSE4.2
  - SandyBridge:      Intel Sandy Bridge (2011) - AVX
  - Haswell:          Intel Haswell (2013) - AVX2
  - Skylake-Client:   Intel Skylake (2015) - AVX2, modern features
  - Cascadelake:      Intel Cascade Lake (2019) - AVX512

Machine type:
  - pc-q35-11.1:      Versioned Q35 chipset contract used by all x86_64 tests

EOF
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Check for QEMU
QEMU_CMD="${XINIM_QEMU_SYSTEM_BIN:-qemu-system-x86_64}"
if ! command -v "${QEMU_CMD}" &> /dev/null; then
    print_error "${QEMU_CMD} not found"
    print_info "Install with: sudo pacman -S qemu-system-x86 (Arch)"
    print_info "Or: sudo apt-get install qemu-system-x86 (Debian/Ubuntu)"
    exit 1
fi

if [[ "${MACHINE}" != "pc-q35-11.1" ]]; then
    print_error "unsupported x86_64 machine: ${MACHINE}"
    print_info "The validated hardware contract is pc-q35-11.1."
    exit 1
fi

# Detect KVM support for acceleration
if [[ -e /dev/kvm ]] && [[ -w /dev/kvm ]]; then
    ACCEL_ARGS=(-accel kvm)
    print_info "KVM acceleration enabled"
else
    print_warning "KVM not available, using software emulation"
    ACCEL_ARGS=(-accel tcg)
fi

# Build QEMU command
QEMU_ARGS=(
    # Machine configuration
    -machine "$MACHINE"
    -cpu "$CPU_TYPE"
    "${ACCEL_ARGS[@]}"
    -m "$MEMORY"
    -smp "$SMP_CPUS"
    
    # The Q35 machine owns its ICH9 AHCI controller. Networking remains off
    # until the selected kernel driver has a proven QEMU transport contract.
    -nodefaults
    -vga none
    -nic none
    
    # Serial port configuration: COM1 for logs, COM2 for kshell
    -serial "$SERIAL_OUTPUT"
    -serial "tcp::${KSHELL_PORT},server,nowait"
    
    # Display
    $DISPLAY

    # Keep stdio free for the selected serial backend.
    -monitor none
    
    # Exit on reboot for clean termination
    -no-reboot
)

if [[ ! -f "${BOOT_IMAGE}" ]]; then
    print_error "Boot image not found: ${BOOT_IMAGE}"
    exit 1
fi
QEMU_ARGS+=(
    -cdrom "${BOOT_IMAGE}"
    -boot d
)

# Add kernel command line if specified
if [[ -n "$KERNEL_CMDLINE" ]]; then
    QEMU_ARGS+=(-append "$KERNEL_CMDLINE")
fi

# Debug mode configuration
if [[ "$DEBUG_MODE" == true ]]; then
    QEMU_ARGS+=(
        -s                        # GDB server on port 1234
        -S                        # Freeze CPU at startup
    )
    if [[ "$GDB_PORT" != "1234" ]]; then
        QEMU_ARGS+=(-gdb "tcp::$GDB_PORT")
    fi
    print_info "Debug mode enabled - GDB server on port $GDB_PORT"
    print_info "Connect with: gdb ${BUILD_ROOT}/xinim -ex 'target remote localhost:$GDB_PORT'"
fi

# Print configuration
print_info "Starting XINIM in QEMU (x86_64)"
print_info "================================"
print_info "Boot Image:   ${BOOT_IMAGE}"
print_info "Memory:       $MEMORY"
print_info "CPUs:         $SMP_CPUS"
print_info "CPU Type:     $CPU_TYPE"
print_info "Machine:      $MACHINE"
print_info "Acceleration: ${ACCEL_ARGS[1]}"
print_info "kshell port:  $KSHELL_PORT (COM2 via TCP)"
print_info "================================"

# Launch QEMU
print_success "Launching QEMU..."
exec "$QEMU_CMD" "${QEMU_ARGS[@]}"
