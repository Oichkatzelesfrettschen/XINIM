#!/usr/bin/env bash
# XINIM QEMU launcher for the i486 GRUB/Multiboot2 init-shell lane.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() { echo -e "${BLUE}[QEMU x86_32]${NC} $1"; }
print_ok() { echo -e "${GREEN}[QEMU x86_32]${NC} $1"; }
print_error() { echo -e "${RED}[QEMU x86_32]${NC} $1"; }

IMAGE_ROOT="${XINIM_IMAGE_ROOT:-${PROJECT_ROOT}/build/i486/Debug/images}"
LOG_ROOT="${XINIM_LOG_ROOT:-${PROJECT_ROOT}/build/i486/Debug/logs}"
BOOT_IMAGE="${XINIM_QEMU_BOOT_IMAGE:-${IMAGE_ROOT}/i486/xinim-i486dx.iso}"
BOOT_DISK=""
DISK_IMAGE="${XINIM_QEMU_DISK_IMAGE:-}"
QEMU_BIN="${XINIM_QEMU_SYSTEM_BIN:-qemu-system-i386}"
MEMORY="64M"
CPU="486"
MACHINE="pc"
VGA="std"
DISPLAY_BACKEND=""
DEBUG_SHELL_PORT="4555"
LOG_FILE="${LOG_ROOT}/qemu-i486.log"

show_help() {
    cat <<EOF
Usage: $0 [--boot-image PATH | --boot-disk PATH] [options]

Boot modes (pick one):
  --boot-image PATH       Boot from ISO image (default)
  --boot-disk PATH        Boot from raw disk image (single-disk boot)

Options:
  --memory SIZE           Guest RAM size (default: 64M)
  --cpu MODEL             QEMU CPU model (default: 486)
  --machine NAME          QEMU machine (default: pc)
  --disk-image PATH       Attach a secondary raw ATA disk image
  --vga TYPE              VGA model (default: std)
  --display BACKEND       QEMU display backend (default: QEMU default)
  --headless              Force headless mode (-display none)
  --log-file PATH         Serial log file (default: $LOG_FILE)
  --debug-shell-port N    COM2 telnet port (default: 4555)
  -h, --help              Show this message

Examples:
  $0 --boot-image "\$XINIM_IMAGE_ROOT/i486/xinim-i486dx.iso"
  $0 --boot-disk xinim-i486-boot.img
  $0 --boot-image boot.iso --disk-image ata.img --cpu pentium --memory 64M
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --boot-image)
            BOOT_IMAGE="${2:?missing path for --boot-image}"
            BOOT_DISK=""
            shift 2
            ;;
        --boot-disk)
            BOOT_DISK="${2:?missing path for --boot-disk}"
            BOOT_IMAGE=""
            shift 2
            ;;
        --memory)
            MEMORY="${2:?missing size for --memory}"
            shift 2
            ;;
        --cpu)
            CPU="${2:?missing CPU model for --cpu}"
            shift 2
            ;;
        --machine)
            MACHINE="${2:?missing machine name for --machine}"
            shift 2
            ;;
        --disk-image)
            DISK_IMAGE="${2:?missing path for --disk-image}"
            shift 2
            ;;
        --vga)
            VGA="${2:?missing VGA type for --vga}"
            shift 2
            ;;
        --display)
            DISPLAY_BACKEND="${2:?missing backend for --display}"
            shift 2
            ;;
        --headless)
            DISPLAY_BACKEND="none"
            shift
            ;;
        --log-file)
            LOG_FILE="${2:?missing path for --log-file}"
            shift 2
            ;;
        --debug-shell-port)
            DEBUG_SHELL_PORT="${2:?missing port for --debug-shell-port}"
            shift 2
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

if ! command -v "${QEMU_BIN}" >/dev/null 2>&1; then
    print_error "${QEMU_BIN} not found"
    exit 1
fi

mkdir -p "$(dirname "${LOG_FILE}")"

print_info "QEMU binary: ${QEMU_BIN}"
print_info "Machine: ${MACHINE}"
print_info "CPU: ${CPU}"
print_info "Memory: ${MEMORY}"
print_info "VGA: ${VGA}"

QEMU_ARGS=(
    -machine "${MACHINE}"
    -cpu "${CPU}"
    -m "${MEMORY}"
    -vga "${VGA}"
    -serial "file:${LOG_FILE}"
    -serial "telnet:127.0.0.1:${DEBUG_SHELL_PORT},server,nowait"
    -monitor none
    -no-reboot
    -no-shutdown
    -netdev user,id=net0
    -device virtio-net-pci,netdev=net0
)

if [[ -n "${BOOT_DISK}" ]]; then
    # Boot from disk image (single-disk mode)
    if [[ ! -f "${BOOT_DISK}" ]]; then
        print_error "Boot disk not found: ${BOOT_DISK}"
        exit 1
    fi
    print_info "Boot disk: ${BOOT_DISK}"
    QEMU_ARGS+=(-boot c)
    QEMU_ARGS+=(-drive "file=${BOOT_DISK},format=raw,index=0,media=disk")
else
    # Boot from ISO (legacy mode)
    if [[ -z "${BOOT_IMAGE}" || ! -f "${BOOT_IMAGE}" ]]; then
        print_error "Boot image not found: ${BOOT_IMAGE}"
        exit 1
    fi
    print_info "Boot image: ${BOOT_IMAGE}"
    QEMU_ARGS+=(-boot d)
    QEMU_ARGS+=(-cdrom "${BOOT_IMAGE}")
fi

if [[ -n "${DISK_IMAGE}" ]]; then
    print_info "Disk: ${DISK_IMAGE}"
    QEMU_ARGS+=(-drive "file=${DISK_IMAGE},format=raw,index=1,media=disk")
fi

if [[ -n "${DISPLAY_BACKEND}" ]]; then
    print_info "Display: ${DISPLAY_BACKEND}"
    QEMU_ARGS+=(-display "${DISPLAY_BACKEND}")
else
    print_info "Display: QEMU default"
fi

print_info "COM1 log: ${LOG_FILE}"
print_info "COM2 telnet: localhost:${DEBUG_SHELL_PORT}"

exec "${QEMU_BIN}" "${QEMU_ARGS[@]}"
