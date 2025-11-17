#!/usr/bin/env bash
# XINIM Binutils Build Script
# Builds binutils 2.41 for x86_64-xinim-elf target
# Part of Week 1: Cross-Compiler Bootstrap

set -euo pipefail

# Load build configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XINIM_ROOT="$(dirname "$SCRIPT_DIR")"

# Source environment if available
if [[ -f "/opt/xinim-toolchain/xinim-env.sh" ]]; then
    source /opt/xinim-toolchain/xinim-env.sh
else
    # Default values
    export XINIM_PREFIX="${XINIM_PREFIX:-/opt/xinim-toolchain}"
    export XINIM_SYSROOT="${XINIM_SYSROOT:-/opt/xinim-sysroot}"
    export XINIM_TARGET="x86_64-xinim-elf"
    export XINIM_BUILD_DIR="${XINIM_BUILD_DIR:-$HOME/xinim-build}"
    export XINIM_SOURCES_DIR="${XINIM_SOURCES_DIR:-$HOME/xinim-sources}"
fi

BINUTILS_VERSION="2.41"
BUILD_DIR="$XINIM_BUILD_DIR/binutils-build"
SOURCE_DIR="$XINIM_SOURCES_DIR/binutils-${BINUTILS_VERSION}"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[binutils]${NC} $1"
}

success() {
    echo -e "${GREEN}[binutils]${NC} $1"
}

error() {
    echo -e "${RED}[binutils]${NC} $1"
}

# Check if source exists
if [[ ! -d "$SOURCE_DIR" ]]; then
    error "Source directory not found: $SOURCE_DIR"
    error "Run setup_build_environment.sh first"
    exit 1
fi

# Create build directory
log "Creating build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure binutils
log "Configuring binutils ${BINUTILS_VERSION} for $XINIM_TARGET..."

"$SOURCE_DIR/configure" \
    --target="$XINIM_TARGET" \
    --prefix="$XINIM_PREFIX" \
    --with-sysroot="$XINIM_SYSROOT" \
    --disable-nls \
    --disable-werror \
    --enable-gold \
    --enable-ld=default \
    --enable-plugins \
    --enable-lto \
    --enable-deterministic-archives \
    --enable-relro \
    --enable-threads \
    --with-pic

success "Configuration complete"

# Build binutils
log "Building binutils (using $(nproc) parallel jobs)..."
make -j$(nproc)

success "Build complete"

# Install binutils
log "Installing binutils to $XINIM_PREFIX..."
make install

success "Installation complete"

# Verify installation
log "Verifying installation..."

TOOLS=(
    "as"
    "ld"
    "ar"
    "nm"
    "objcopy"
    "objdump"
    "ranlib"
    "readelf"
    "size"
    "strings"
    "strip"
)

ALL_OK=true
for tool in "${TOOLS[@]}"; do
    TOOL_PATH="$XINIM_PREFIX/bin/$XINIM_TARGET-$tool"
    if [[ -x "$TOOL_PATH" ]]; then
        VERSION=$("$TOOL_PATH" --version | head -n1)
        success "  ✓ $tool: $VERSION"
    else
        error "  ✗ $tool: NOT FOUND"
        ALL_OK=false
    fi
done

if $ALL_OK; then
    echo
    success "================================================"
    success "binutils ${BINUTILS_VERSION} build successful!"
    success "================================================"
    echo
    log "Installed tools in: $XINIM_PREFIX/bin"
    log "Next step: Build GCC Stage 1"
    log "  ./build_gcc_stage1.sh"
    echo
else
    error "Some tools are missing. Build may have failed."
    exit 1
fi
