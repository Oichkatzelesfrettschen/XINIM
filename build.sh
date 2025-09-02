#!/usr/bin/env bash
#
# build.sh - Unified Multi-Build System for XINIM
#
# This script supports multiple build systems:
# 1. Native XINIM build tool (C++20 based)
# 2. CMake with preset profiles
# 3. xmake (if available)
#
# Usage:
#    ./build.sh [--system=<native|cmake|xmake>] [--profile=<profile>] [options]
#    ./build.sh --help
#
# Build Systems:
#    native  - Use XINIM's native C++20 build tool
#    cmake   - Use CMake with Ninja generator
#    xmake   - Use xmake build system
#
# Profiles (CMake):
#    developer   - Debug build with sanitizers
#    performance - Release optimized for host CPU
#    release     - Production build with LTO
#
# Options:
#    --arch=<x86_64|arm64>  - Target architecture
#    --qemu                 - Build for QEMU emulation
#    --clean                - Clean build artifacts
#    --rebuild              - Clean and rebuild

set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
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
    echo -e "${RED}[ERROR]${NC} $*"
}

show_help() {
    grep '^#' "$0" | head -30 | tail -n +2 | sed -E 's/^# ?//'
    exit 0
}

# Default settings
build_system="auto"
profile="developer"
target_arch=""
build_qemu=false
clean_build=false
rebuild=false
build_dir="build"

# Native build system settings
BUILD_TOOL_SOURCE="${SCRIPT_DIR}/tools/xinim_build_simple.cpp"
BUILD_TOOL_BINARY="${SCRIPT_DIR}/xinim_build"

# Parse arguments
while [ $# -gt 0 ]; do
    case $1 in
    --system=*)
        build_system="${1#*=}"
        ;;
    --profile=*)
        profile="${1#*=}"
        ;;
    --arch=*)
        target_arch="${1#*=}"
        ;;
    --qemu)
        build_qemu=true
        ;;
    --clean)
        clean_build=true
        ;;
    --rebuild)
        rebuild=true
        ;;
    --build-dir=*)
        build_dir="${1#*=}"
        ;;
    --help | -h)
        show_help
        ;;
    *)
        break
        ;;
    esac
    shift
done

# Auto-detect build system if not specified
detect_build_system() {
    if [[ "$build_system" == "auto" ]]; then
        if command -v xmake >/dev/null 2>&1; then
            log_info "Auto-detected xmake"
            build_system="xmake"
        elif command -v cmake >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
            log_info "Auto-detected CMake with Ninja"
            build_system="cmake"
        else
            log_info "Using native build system"
            build_system="native"
        fi
    fi
}

# Check for C++23 compiler
check_compiler() {
    log_info "Checking for C++23 compiler support..."
    
    if command -v clang++ >/dev/null 2>&1; then
        local version=$(clang++ --version | head -1)
        log_success "Found Clang: $version"
        return 0
    elif command -v g++ >/dev/null 2>&1; then
        local version=$(g++ --version | head -1)
        if [[ $version =~ ([0-9]+)\.([0-9]+) ]] && [[ ${BASH_REMATCH[1]} -ge 13 ]]; then
            log_success "Found GCC: $version"
            return 0
        fi
    fi
    log_error "No suitable C++23 compiler found!"
    return 1
}

# Native build system
build_native() {
    log_info "Using XINIM native build system"
    
    local compiler="clang++"
    if ! command -v clang++ >/dev/null 2>&1; then
        compiler="g++"
    fi
    
    # Compile native build tool if needed
    if [[ ! -x "$BUILD_TOOL_BINARY" ]] || [[ "$BUILD_TOOL_SOURCE" -nt "$BUILD_TOOL_BINARY" ]]; then
        log_info "Compiling native build tool..."
        $compiler -std=c++20 -O2 -Wall -Wextra -o "$BUILD_TOOL_BINARY" "$BUILD_TOOL_SOURCE"
        log_success "Native build tool compiled"
    fi
    
    # Run native build
    local build_args=()
    [[ -n "$target_arch" ]] && build_args+=(--arch="$target_arch")
    [[ "$build_qemu" == true ]] && build_args+=(--qemu)
    
    "$BUILD_TOOL_BINARY" "${build_args[@]}"
}

# CMake build system
build_cmake() {
    log_info "Using CMake build system with profile: $profile"
    
    local cmake_flags=(
        -S .
        -G Ninja
    )
    
    # Detect compiler
    if command -v clang-18 >/dev/null 2>&1; then
        cmake_flags+=(-DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18)
    elif command -v clang >/dev/null 2>&1; then
        cmake_flags+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
    fi
    
    # Architecture flags
    if [[ -n "$target_arch" ]]; then
        cmake_flags+=(-DXINIM_TARGET_ARCH="$target_arch")
    fi
    
    # QEMU support
    if [[ "$build_qemu" == true ]]; then
        cmake_flags+=(-DXINIM_BUILD_QEMU=ON)
    fi
    
    # Profile settings
    case "$profile" in
        developer)
            cmake_flags+=(
                -B "$build_dir/developer"
                -DCMAKE_BUILD_TYPE=Debug
                -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer"
            )
            ;;
        performance)
            cmake_flags+=(
                -B "$build_dir/performance"
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto"
            )
            ;;
        release)
            cmake_flags+=(
                -B "$build_dir/release"
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
                -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG -flto"
            )
            ;;
    esac
    
    cmake "${cmake_flags[@]}"
    cmake --build "$build_dir/$profile"
}

# xmake build system
build_xmake() {
    log_info "Using xmake build system"
    
    local xmake_args=()
    
    # Configure xmake
    if [[ -n "$target_arch" ]]; then
        xmake_args+=(--arch="$target_arch")
    fi
    
    case "$profile" in
        developer)
            xmake_args+=(-m debug)
            ;;
        performance|release)
            xmake_args+=(-m release)
            ;;
    esac
    
    xmake f "${xmake_args[@]}"
    xmake
    
    # Build QEMU image if requested
    if [[ "$build_qemu" == true ]]; then
        log_info "Building QEMU images..."
        xmake run qemu_build
    fi
}

# Clean build artifacts
clean_artifacts() {
    log_info "Cleaning build artifacts..."
    rm -rf "$build_dir"
    rm -f "$BUILD_TOOL_BINARY"
    [[ -d .xmake ]] && rm -rf .xmake
    log_success "Clean completed"
}

# Main execution
main() {
    cd "$SCRIPT_DIR"
    
    # Handle clean/rebuild
    if [[ "$clean_build" == true ]] || [[ "$rebuild" == true ]]; then
        clean_artifacts
        [[ "$clean_build" == true ]] && exit 0
    fi
    
    # Check compiler
    if ! check_compiler; then
        log_error "Cannot proceed without C++23 compiler"
        exit 1
    fi
    
    # Detect build system
    detect_build_system
    
    log_info "XINIM Multi-Architecture Build System"
    log_info "====================================="
    log_info "Build System: $build_system"
    log_info "Profile: $profile"
    [[ -n "$target_arch" ]] && log_info "Target Architecture: $target_arch"
    [[ "$build_qemu" == true ]] && log_info "QEMU Build: Enabled"
    
    # Execute build
    case "$build_system" in
        native)
            build_native
            ;;
        cmake)
            build_cmake
            ;;
        xmake)
            build_xmake
            ;;
        *)
            log_error "Unknown build system: $build_system"
            exit 1
            ;;
    esac
    
    local exit_code=$?
    if [[ $exit_code -eq 0 ]]; then
        log_success "Build completed successfully!"
        
        # Show QEMU run instructions if built
        if [[ "$build_qemu" == true ]]; then
            echo ""
            log_info "To run in QEMU:"
            echo "  x86_64: ./tools/run_qemu.sh x86_64"
            echo "  ARM64:  ./tools/run_qemu.sh arm64"
        fi
    else
        log_error "Build failed with exit code $exit_code"
    fi
    
    return $exit_code
}

# Run main
main "$@"
