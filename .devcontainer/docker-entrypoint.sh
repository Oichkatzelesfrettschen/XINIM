#!/bin/bash
# Docker entrypoint script for XINIM i386 build environment

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   XINIM i386 Build Environment Ready      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}Compiler:${NC}     $(clang++ --version | head -n1)"
echo -e "${GREEN}Architecture:${NC} i386 (32-bit x86)"
echo -e "${GREEN}Build Tool:${NC}   xmake $(xmake --version | head -n1)"
echo -e "${GREEN}QEMU:${NC}         $(qemu-system-i386 --version | head -n1)"
echo ""
echo -e "${BLUE}Quick Start Commands:${NC}"
echo -e "  ${GREEN}xmake config --arch=i386${NC}       Configure for i386 build"
echo -e "  ${GREEN}xmake build${NC}                    Build XINIM kernel"
echo -e "  ${GREEN}./scripts/qemu_i386.sh${NC}         Run in QEMU"
echo -e "  ${GREEN}./scripts/qemu_i386.sh -g${NC}      Debug with GDB"
echo ""

# Execute the command passed to docker run
exec "$@"
