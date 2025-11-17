#!/usr/bin/env bash
# XINIM Build Environment Setup Script
# Sets up cross-compiler toolchain infrastructure for x86_64-xinim-elf
# Part of SUSv4 POSIX.1-2017 Compliance Implementation - Week 1

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
XINIM_PREFIX="${XINIM_PREFIX:-/opt/xinim-toolchain}"
XINIM_SYSROOT="${XINIM_SYSROOT:-/opt/xinim-sysroot}"
XINIM_TARGET="x86_64-xinim-elf"
XINIM_BUILD_DIR="${XINIM_BUILD_DIR:-$HOME/xinim-build}"
XINIM_SOURCES_DIR="${XINIM_SOURCES_DIR:-$HOME/xinim-sources}"

# Versions
BINUTILS_VERSION="2.41"
GCC_VERSION="13.2.0"
MUSL_VERSION="1.2.4"
LINUX_HEADERS_VERSION="6.6"
LLVM_VERSION="18.1.0"

# URLs
BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
MUSL_URL="https://musl.libc.org/releases/musl-${MUSL_VERSION}.tar.gz"
LINUX_HEADERS_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${LINUX_HEADERS_VERSION}.tar.xz"

# Logging
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -eq 0 ]]; then
        log_warning "Running as root. This is required for installing to /opt"
    else
        log_info "Not running as root. Will use sudo for privileged operations."
        # Check if sudo is available
        if ! command -v sudo &> /dev/null; then
            log_error "sudo not found and not running as root. Cannot install to /opt"
            exit 1
        fi
    fi
}

# Check system dependencies
check_dependencies() {
    log_info "Checking system dependencies..."

    local missing_deps=()
    local required_deps=(
        "make"
        "gcc"
        "g++"
        "wget"
        "tar"
        "xz"
        "gzip"
        "bison"
        "flex"
        "texinfo"
        "help2man"
        "gawk"
        "libc6-dev"
    )

    for dep in "${required_deps[@]}"; do
        if ! command -v "$dep" &> /dev/null && ! dpkg -l | grep -q "^ii.*$dep"; then
            missing_deps+=("$dep")
        fi
    done

    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Install with: sudo apt-get install ${missing_deps[*]}"

        read -p "Would you like to install missing dependencies now? [y/N] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            sudo apt-get update
            sudo apt-get install -y "${missing_deps[@]}"
        else
            log_error "Cannot continue without dependencies"
            exit 1
        fi
    else
        log_success "All dependencies satisfied"
    fi
}

# Create directory structure
create_directories() {
    log_info "Creating directory structure..."

    # Privileged directories (need sudo)
    if [[ ! -d "$XINIM_PREFIX" ]]; then
        sudo mkdir -p "$XINIM_PREFIX"
        sudo chown -R "$USER:$USER" "$XINIM_PREFIX"
        log_success "Created $XINIM_PREFIX"
    fi

    if [[ ! -d "$XINIM_SYSROOT" ]]; then
        sudo mkdir -p "$XINIM_SYSROOT"
        sudo chown -R "$USER:$USER" "$XINIM_SYSROOT"
        log_success "Created $XINIM_SYSROOT"
    fi

    # Sysroot directory structure
    mkdir -p "$XINIM_SYSROOT"/{lib,usr/{include,lib,bin,sbin},bin,sbin,etc,var,tmp,dev,proc,sys}

    # User directories (no sudo needed)
    mkdir -p "$XINIM_BUILD_DIR"
    mkdir -p "$XINIM_SOURCES_DIR"

    log_success "Directory structure created"
}

# Download sources
download_sources() {
    log_info "Downloading toolchain sources..."

    cd "$XINIM_SOURCES_DIR"

    # Download binutils
    if [[ ! -f "binutils-${BINUTILS_VERSION}.tar.xz" ]]; then
        log_info "Downloading binutils ${BINUTILS_VERSION}..."
        wget -c "$BINUTILS_URL"
        log_success "Downloaded binutils"
    else
        log_info "binutils ${BINUTILS_VERSION} already downloaded"
    fi

    # Download GCC
    if [[ ! -f "gcc-${GCC_VERSION}.tar.xz" ]]; then
        log_info "Downloading GCC ${GCC_VERSION}..."
        wget -c "$GCC_URL"
        log_success "Downloaded GCC"
    else
        log_info "GCC ${GCC_VERSION} already downloaded"
    fi

    # Download musl libc
    if [[ ! -f "musl-${MUSL_VERSION}.tar.gz" ]]; then
        log_info "Downloading musl libc ${MUSL_VERSION}..."
        wget -c "$MUSL_URL"
        log_success "Downloaded musl libc"
    else
        log_info "musl libc ${MUSL_VERSION} already downloaded"
    fi

    # Download Linux headers (for reference, not strictly needed for freestanding)
    if [[ ! -f "linux-${LINUX_HEADERS_VERSION}.tar.xz" ]]; then
        log_info "Downloading Linux headers ${LINUX_HEADERS_VERSION}..."
        wget -c "$LINUX_HEADERS_URL"
        log_success "Downloaded Linux headers"
    else
        log_info "Linux headers ${LINUX_HEADERS_VERSION} already downloaded"
    fi
}

# Extract sources
extract_sources() {
    log_info "Extracting sources..."

    cd "$XINIM_SOURCES_DIR"

    # Extract binutils
    if [[ ! -d "binutils-${BINUTILS_VERSION}" ]]; then
        log_info "Extracting binutils..."
        tar xf "binutils-${BINUTILS_VERSION}.tar.xz"
        log_success "Extracted binutils"
    fi

    # Extract GCC
    if [[ ! -d "gcc-${GCC_VERSION}" ]]; then
        log_info "Extracting GCC..."
        tar xf "gcc-${GCC_VERSION}.tar.xz"
        log_success "Extracted GCC"
    fi

    # Extract musl
    if [[ ! -d "musl-${MUSL_VERSION}" ]]; then
        log_info "Extracting musl libc..."
        tar xf "musl-${MUSL_VERSION}.tar.gz"
        log_success "Extracted musl libc"
    fi

    # Extract Linux headers
    if [[ ! -d "linux-${LINUX_HEADERS_VERSION}" ]]; then
        log_info "Extracting Linux headers..."
        tar xf "linux-${LINUX_HEADERS_VERSION}.tar.xz"
        log_success "Extracted Linux headers"
    fi
}

# Download GCC prerequisites
download_gcc_prerequisites() {
    log_info "Downloading GCC prerequisites (GMP, MPFR, MPC, ISL)..."

    cd "$XINIM_SOURCES_DIR/gcc-${GCC_VERSION}"

    if [[ ! -d "gmp" ]] || [[ ! -d "mpfr" ]] || [[ ! -d "mpc" ]] || [[ ! -d "isl" ]]; then
        ./contrib/download_prerequisites
        log_success "Downloaded GCC prerequisites"
    else
        log_info "GCC prerequisites already downloaded"
    fi
}

# Create environment script
create_environment_script() {
    log_info "Creating environment script..."

    cat > "$XINIM_PREFIX/xinim-env.sh" <<'EOF'
#!/usr/bin/env bash
# XINIM Build Environment Variables
# Source this file to set up the build environment

export XINIM_PREFIX="/opt/xinim-toolchain"
export XINIM_SYSROOT="/opt/xinim-sysroot"
export XINIM_TARGET="x86_64-xinim-elf"
export XINIM_BUILD_DIR="$HOME/xinim-build"
export XINIM_SOURCES_DIR="$HOME/xinim-sources"

# Add toolchain to PATH
export PATH="$XINIM_PREFIX/bin:$PATH"

# Compiler flags for XINIM target
export XINIM_CFLAGS="-march=x86-64 -mtune=generic -O2 -pipe"
export XINIM_CXXFLAGS="$XINIM_CFLAGS"
export XINIM_LDFLAGS="-Wl,-z,relro,-z,now"

# Cross-compiler variables
export CC="$XINIM_PREFIX/bin/$XINIM_TARGET-gcc"
export CXX="$XINIM_PREFIX/bin/$XINIM_TARGET-g++"
export AR="$XINIM_PREFIX/bin/$XINIM_TARGET-ar"
export AS="$XINIM_PREFIX/bin/$XINIM_TARGET-as"
export LD="$XINIM_PREFIX/bin/$XINIM_TARGET-ld"
export NM="$XINIM_PREFIX/bin/$XINIM_TARGET-nm"
export OBJCOPY="$XINIM_PREFIX/bin/$XINIM_TARGET-objcopy"
export OBJDUMP="$XINIM_PREFIX/bin/$XINIM_TARGET-objdump"
export RANLIB="$XINIM_PREFIX/bin/$XINIM_TARGET-ranlib"
export STRIP="$XINIM_PREFIX/bin/$XINIM_TARGET-strip"

echo "XINIM build environment loaded"
echo "Toolchain: $XINIM_PREFIX"
echo "Sysroot: $XINIM_SYSROOT"
echo "Target: $XINIM_TARGET"
EOF

    chmod +x "$XINIM_PREFIX/xinim-env.sh"

    log_success "Created environment script: $XINIM_PREFIX/xinim-env.sh"
    log_info "Source this file to set up the build environment:"
    log_info "  source $XINIM_PREFIX/xinim-env.sh"
}

# Create build configuration file
create_build_config() {
    log_info "Creating build configuration file..."

    cat > "$XINIM_BUILD_DIR/build.conf" <<EOF
# XINIM Build Configuration
# Generated by setup_build_environment.sh

# Directories
XINIM_PREFIX="$XINIM_PREFIX"
XINIM_SYSROOT="$XINIM_SYSROOT"
XINIM_TARGET="$XINIM_TARGET"
XINIM_BUILD_DIR="$XINIM_BUILD_DIR"
XINIM_SOURCES_DIR="$XINIM_SOURCES_DIR"

# Versions
BINUTILS_VERSION="$BINUTILS_VERSION"
GCC_VERSION="$GCC_VERSION"
MUSL_VERSION="$MUSL_VERSION"
LINUX_HEADERS_VERSION="$LINUX_HEADERS_VERSION"
LLVM_VERSION="$LLVM_VERSION"

# Build options
MAKE_JOBS="$(nproc)"
ENABLE_GOLD="yes"
ENABLE_LTO="yes"
ENABLE_PLUGINS="yes"

# Target architecture
TARGET_ARCH="x86_64"
TARGET_BASELINE="x86-64"  # x86-64-v1
TARGET_TUNE="generic"

# Optimization
OPT_LEVEL="-O2"
DEBUG_INFO="-g"
STRIP_BINARIES="yes"
EOF

    log_success "Created build configuration: $XINIM_BUILD_DIR/build.conf"
}

# Display summary
display_summary() {
    log_success "Build environment setup complete!"
    echo
    echo "========================================"
    echo "XINIM Build Environment Summary"
    echo "========================================"
    echo "Toolchain prefix:  $XINIM_PREFIX"
    echo "Sysroot:           $XINIM_SYSROOT"
    echo "Target:            $XINIM_TARGET"
    echo "Build directory:   $XINIM_BUILD_DIR"
    echo "Sources directory: $XINIM_SOURCES_DIR"
    echo
    echo "Versions:"
    echo "  binutils: $BINUTILS_VERSION"
    echo "  GCC:      $GCC_VERSION"
    echo "  musl:     $MUSL_VERSION"
    echo "  Linux:    $LINUX_HEADERS_VERSION (headers)"
    echo
    echo "Next steps:"
    echo "  1. Source the environment:"
    echo "     source $XINIM_PREFIX/xinim-env.sh"
    echo
    echo "  2. Build binutils:"
    echo "     cd $XINIM_ROOT/scripts"
    echo "     ./build_binutils.sh"
    echo
    echo "  3. Build GCC Stage 1:"
    echo "     ./build_gcc_stage1.sh"
    echo
    echo "  4. Build musl libc:"
    echo "     ./build_musl.sh"
    echo
    echo "  5. Build GCC Stage 2:"
    echo "     ./build_gcc_stage2.sh"
    echo "========================================"
}

# Main execution
main() {
    log_info "XINIM Build Environment Setup"
    log_info "=============================="
    echo

    check_root
    check_dependencies
    create_directories
    download_sources
    extract_sources
    download_gcc_prerequisites
    create_environment_script
    create_build_config
    display_summary
}

# Run main
main "$@"
