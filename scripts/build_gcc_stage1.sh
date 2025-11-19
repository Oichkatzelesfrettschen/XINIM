#!/usr/bin/env bash
# XINIM GCC Stage 1 Build Script
# Builds GCC 13.2 Stage 1 (bootstrap compiler, no libc) for x86_64-xinim-elf
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
    export PATH="$XINIM_PREFIX/bin:$PATH"
fi

GCC_VERSION="13.2.0"
BUILD_DIR="$XINIM_BUILD_DIR/gcc-stage1-build"
SOURCE_DIR="$XINIM_SOURCES_DIR/gcc-${GCC_VERSION}"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[gcc-stage1]${NC} $1"
}

success() {
    echo -e "${GREEN}[gcc-stage1]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[gcc-stage1]${NC} $1"
}

error() {
    echo -e "${RED}[gcc-stage1]${NC} $1"
}

# Check if binutils is installed
if [[ ! -x "$XINIM_PREFIX/bin/$XINIM_TARGET-as" ]]; then
    error "binutils not found. Build binutils first:"
    error "  ./build_binutils.sh"
    exit 1
fi

# Check if source exists
if [[ ! -d "$SOURCE_DIR" ]]; then
    error "Source directory not found: $SOURCE_DIR"
    error "Run setup_build_environment.sh first"
    exit 1
fi

# Check if prerequisites are downloaded
if [[ ! -d "$SOURCE_DIR/gmp" ]] || [[ ! -d "$SOURCE_DIR/mpfr" ]] || [[ ! -d "$SOURCE_DIR/mpc" ]]; then
    error "GCC prerequisites not found. Run setup_build_environment.sh first"
    exit 1
fi

# Create build directory
log "Creating build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure GCC Stage 1
log "Configuring GCC ${GCC_VERSION} Stage 1 for $XINIM_TARGET..."
warn "This is a bootstrap compiler without libc support"

"$SOURCE_DIR/configure" \
    --target="$XINIM_TARGET" \
    --prefix="$XINIM_PREFIX" \
    --with-sysroot="$XINIM_SYSROOT" \
    --enable-languages=c,c++ \
    --without-headers \
    --with-newlib \
    --disable-nls \
    --disable-shared \
    --disable-multilib \
    --disable-decimal-float \
    --disable-threads \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libquadmath \
    --disable-libssp \
    --disable-libvtv \
    --disable-libstdcxx \
    --enable-lto \
    --with-arch=x86-64 \
    --with-tune=generic

success "Configuration complete"

# Build GCC Stage 1
log "Building GCC Stage 1 (using $(nproc) parallel jobs)..."
log "This will take approximately 20-30 minutes..."

# Build only the compiler and target-libgcc
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc

success "Build complete"

# Install GCC Stage 1
log "Installing GCC Stage 1 to $XINIM_PREFIX..."
make install-gcc
make install-target-libgcc

success "Installation complete"

# Verify installation
log "Verifying installation..."

COMPILERS=(
    "gcc"
    "g++"
    "cpp"
)

ALL_OK=true
for compiler in "${COMPILERS[@]}"; do
    COMPILER_PATH="$XINIM_PREFIX/bin/$XINIM_TARGET-$compiler"
    if [[ -x "$COMPILER_PATH" ]]; then
        VERSION=$("$COMPILER_PATH" --version | head -n1)
        success "  ✓ $compiler: $VERSION"
    else
        error "  ✗ $compiler: NOT FOUND"
        ALL_OK=false
    fi
done

# Test compilation (freestanding)
log "Testing freestanding compilation..."

cat > /tmp/test_stage1.c <<'EOF'
void _start(void) {
    /* Freestanding program - no libc */
    while(1);
}
EOF

if "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" -c /tmp/test_stage1.c -o /tmp/test_stage1.o; then
    success "  ✓ Can compile freestanding C code"
else
    error "  ✗ Compilation test failed"
    ALL_OK=false
fi

rm -f /tmp/test_stage1.c /tmp/test_stage1.o

if $ALL_OK; then
    echo
    success "================================================"
    success "GCC ${GCC_VERSION} Stage 1 build successful!"
    success "================================================"
    echo
    log "Installed compilers in: $XINIM_PREFIX/bin"
    warn "NOTE: This is a bootstrap compiler without libc"
    warn "It can only compile freestanding code (kernel, bootloader)"
    echo
    log "Next step: Build musl libc"
    log "  ./build_musl.sh"
    echo
else
    error "Some components are missing. Build may have failed."
    exit 1
fi
