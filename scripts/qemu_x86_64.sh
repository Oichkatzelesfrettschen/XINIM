#!/bin/bash
# XINIM QEMU Launch Script for x86_64
# Optimized settings for running XINIM kernel in x86_64 QEMU session

set -e

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
KERNEL_IMAGE="${PROJECT_ROOT}/build/xinim"
MEMORY="512M"
CPU_TYPE="qemu64"
MACHINE="q35"  # Modern PC with PCIe
ACCEL=""
SERIAL_OUTPUT="stdio"
DISPLAY="-nographic"
DEBUG_MODE=false
GDB_PORT=1234
KERNEL_CMDLINE=""
SMP_CPUS="2"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -k|--kernel)
            KERNEL_IMAGE="$2"
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
        --cmdline)
            KERNEL_CMDLINE="$2"
            shift 2
            ;;
        -h|--help)
            cat << EOF
Usage: $0 [OPTIONS]

XINIM x86_64 QEMU Launch Script

Options:
  -k, --kernel PATH       Path to kernel image (default: build/xinim)
  -m, --memory SIZE       Memory size (default: 512M)
  --cpu TYPE             CPU type (default: qemu64)
                         Options: qemu64, host, Nehalem, SandyBridge, IvyBridge,
                                  Haswell, Broadwell, Skylake-Client, Cascadelake-Server
  --machine TYPE         Machine type (default: q35)
                         Options: q35 (modern PCIe), pc (legacy i440FX)
  --smp N                Number of CPUs (default: 2)
  -g, --debug            Enable GDB debugging
  --gdb-port PORT        GDB server port (default: 1234)
  --display              Enable graphical display (default: serial only)
  --cmdline "ARGS"       Kernel command line arguments
  -h, --help             Show this help message

Examples:
  # Basic boot
  $0

  # Boot with more memory and CPUs
  $0 -m 2G --smp 4

  # Boot with modern Skylake CPU features
  $0 --cpu Skylake-Client

  # Debug mode with GDB
  $0 -g

  # Boot with host CPU passthrough (requires KVM)
  $0 --cpu host

Recommended CPU types for x86_64:
  - qemu64:           Generic 64-bit x86 (best compatibility)
  - host:             Pass through host CPU features (requires KVM)
  - Nehalem:          Intel Core i7 (2008) - SSE4.2
  - SandyBridge:      Intel Sandy Bridge (2011) - AVX
  - Haswell:          Intel Haswell (2013) - AVX2
  - Skylake-Client:   Intel Skylake (2015) - AVX2, modern features
  - Cascadelake:      Intel Cascade Lake (2019) - AVX512

Machine types:
  - q35:              Modern PC (Q35 chipset, PCIe) - recommended
  - pc:               Standard PC (i440FX chipset, legacy PCI)

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

# Check if kernel image exists
if [[ ! -f "$KERNEL_IMAGE" ]]; then
    print_error "Kernel image not found: $KERNEL_IMAGE"
    print_info "Please build the kernel first with: xmake build"
    exit 1
fi

# Check for QEMU
if ! command -v qemu-system-x86_64 &> /dev/null; then
    print_error "qemu-system-x86_64 not found"
    print_info "Install with: sudo apt-get install qemu-system-x86"
    exit 1
fi

# Detect KVM support for acceleration
if [[ -e /dev/kvm ]] && [[ -w /dev/kvm ]]; then
    ACCEL="-accel kvm"
    print_info "KVM acceleration enabled"
else
    print_warning "KVM not available, using software emulation"
    ACCEL="-accel tcg"
fi

# Build QEMU command
QEMU_CMD="qemu-system-x86_64"
QEMU_ARGS=(
    # Machine configuration
    -machine "$MACHINE"
    -cpu "$CPU_TYPE"
    $ACCEL
    -m "$MEMORY"
    -smp "$SMP_CPUS"
    
    # Boot kernel directly (multiboot)
    -kernel "$KERNEL_IMAGE"
    
    # Modern PC devices
    -device "ahci,id=ahci"                    # AHCI controller
    -device "e1000,netdev=net0"               # E1000 network card
    -netdev "user,id=net0"                    # User-mode networking
    
    # Serial port configuration
    -serial "$SERIAL_OUTPUT"
    
    # Display
    $DISPLAY
    
    # Additional useful options
    -no-reboot                                # Exit on reboot
    -no-shutdown                              # Keep QEMU running after guest shutdown
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
    print_info "Connect with: gdb $KERNEL_IMAGE -ex 'target remote localhost:$GDB_PORT'"
fi

# Print configuration
print_info "Starting XINIM in QEMU (x86_64)"
print_info "================================"
print_info "Kernel:       $KERNEL_IMAGE"
print_info "Memory:       $MEMORY"
print_info "CPUs:         $SMP_CPUS"
print_info "CPU Type:     $CPU_TYPE"
print_info "Machine:      $MACHINE"
print_info "Acceleration: ${ACCEL#-accel }"
print_info "================================"

# Launch QEMU
print_success "Launching QEMU..."
exec "$QEMU_CMD" "${QEMU_ARGS[@]}"
