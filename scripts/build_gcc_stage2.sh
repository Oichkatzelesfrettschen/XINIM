#!/usr/bin/env bash
# XINIM GCC Stage 2 Build Script
# Builds GCC 13.2 Stage 2 (full compiler with libc) for x86_64-xinim-elf
# Part of Week 3: Full Toolchain Completion

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
BUILD_DIR="$XINIM_BUILD_DIR/gcc-stage2-build"
SOURCE_DIR="$XINIM_SOURCES_DIR/gcc-${GCC_VERSION}"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[gcc-stage2]${NC} $1"
}

success() {
    echo -e "${GREEN}[gcc-stage2]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[gcc-stage2]${NC} $1"
}

error() {
    echo -e "${RED}[gcc-stage2]${NC} $1"
}

# Check if musl is installed
if [[ ! -f "$XINIM_SYSROOT/lib/libc.a" ]]; then
    error "musl libc not found. Build musl first:"
    error "  ./build_musl.sh"
    exit 1
fi

# Check if GCC Stage 1 is installed
if [[ ! -x "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" ]]; then
    error "GCC Stage 1 not found. Build GCC Stage 1 first:"
    error "  ./build_gcc_stage1.sh"
    exit 1
fi

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

# Configure GCC Stage 2
log "Configuring GCC ${GCC_VERSION} Stage 2 for $XINIM_TARGET..."
log "This is the full compiler with C++ standard library support"

"$SOURCE_DIR/configure" \
    --target="$XINIM_TARGET" \
    --prefix="$XINIM_PREFIX" \
    --with-sysroot="$XINIM_SYSROOT" \
    --enable-languages=c,c++ \
    --disable-nls \
    --enable-shared \
    --enable-static \
    --disable-multilib \
    --enable-threads=posix \
    --enable-tls \
    --enable-lto \
    --enable-plugins \
    --enable-gold \
    --enable-ld=default \
    --enable-libstdcxx \
    --enable-libstdcxx-time=yes \
    --enable-libstdcxx-filesystem-ts=yes \
    --with-arch=x86-64 \
    --with-tune=generic \
    --with-fpmath=sse \
    --enable-clocale=generic \
    --disable-werror

success "Configuration complete"

# Build GCC Stage 2
log "Building GCC Stage 2 (using $(nproc) parallel jobs)..."
log "This will take approximately 30-60 minutes..."

make -j$(nproc)

success "Build complete"

# Install GCC Stage 2
log "Installing GCC Stage 2 to $XINIM_PREFIX..."
make install

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

# Check for libstdc++
LIBSTDCXX_PATH="$XINIM_SYSROOT/lib/libstdc++.a"
if [[ -f "$LIBSTDCXX_PATH" ]]; then
    LIBSTDCXX_SIZE=$(du -h "$LIBSTDCXX_PATH" | cut -f1)
    success "  ✓ libstdc++.a: $LIBSTDCXX_SIZE"
else
    error "  ✗ libstdc++.a: NOT FOUND"
    ALL_OK=false
fi

# Check for libgcc
LIBGCC_PATH="$XINIM_PREFIX/lib/gcc/$XINIM_TARGET/$GCC_VERSION/libgcc.a"
if [[ -f "$LIBGCC_PATH" ]]; then
    LIBGCC_SIZE=$(du -h "$LIBGCC_PATH" | cut -f1)
    success "  ✓ libgcc.a: $LIBGCC_SIZE"
else
    error "  ✗ libgcc.a: NOT FOUND"
    ALL_OK=false
fi

# Test C compilation
log "Testing C compilation..."

cat > /tmp/test_stage2.c <<'EOF'
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Hello from GCC Stage 2!\n");
    return 0;
}
EOF

if "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" \
    --sysroot="$XINIM_SYSROOT" \
    -static \
    /tmp/test_stage2.c \
    -o /tmp/test_stage2 2>/dev/null; then
    success "  ✓ Can compile C programs with libc"
else
    warn "  ⚠ C compilation test failed"
    ALL_OK=false
fi

rm -f /tmp/test_stage2.c /tmp/test_stage2

# Test C++ compilation
log "Testing C++ compilation..."

cat > /tmp/test_stage2.cpp <<'EOF'
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::vector<std::string> vec = {"Hello", "from", "C++23!"};
    for (const auto& str : vec) {
        std::cout << str << " ";
    }
    std::cout << std::endl;
    return 0;
}
EOF

if "$XINIM_PREFIX/bin/$XINIM_TARGET-g++" \
    --sysroot="$XINIM_SYSROOT" \
    -std=c++23 \
    -static \
    /tmp/test_stage2.cpp \
    -o /tmp/test_stage2_cpp 2>/dev/null; then
    success "  ✓ Can compile C++23 programs with libstdc++"
else
    warn "  ⚠ C++ compilation test failed"
    ALL_OK=false
fi

rm -f /tmp/test_stage2.cpp /tmp/test_stage2_cpp

if $ALL_OK; then
    echo
    success "================================================"
    success "GCC ${GCC_VERSION} Stage 2 build successful!"
    success "================================================"
    echo
    log "Installed compilers in: $XINIM_PREFIX/bin"
    log "Sysroot: $XINIM_SYSROOT"
    log "Libraries:"
    log "  - libc.a (musl): $(du -h $XINIM_SYSROOT/lib/libc.a | cut -f1)"
    log "  - libstdc++.a: $(du -h $LIBSTDCXX_PATH | cut -f1)"
    log "  - libgcc.a: $(du -h $LIBGCC_PATH | cut -f1)"
    echo
    success "Full cross-compiler toolchain is now ready!"
    echo
    log "You can now:"
    log "  1. Build XINIM kernel with the new toolchain"
    log "  2. Build mksh shell (Week 4)"
    log "  3. Start implementing syscalls (Week 5)"
    echo
    log "To use the toolchain:"
    log "  source $XINIM_PREFIX/xinim-env.sh"
    log "  $XINIM_TARGET-gcc --sysroot=\$XINIM_SYSROOT program.c"
    echo
else
    error "Some components are missing. Build may have failed."
    exit 1
fi
