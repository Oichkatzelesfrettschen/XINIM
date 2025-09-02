#!/bin/bash

# XINIM Native Build System Bootstrap Script
# BSD 2-Clause License

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TOOL_SOURCE="${SCRIPT_DIR}/tools/xinim_build_simple.cpp"
BUILD_TOOL_BINARY="${SCRIPT_DIR}/xinim_build"

# Colors
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

# Check if C++23 compiler is available
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
        else
            log_warning "GCC version may not support C++23 fully: $version"
        fi
    else
        log_error "No suitable C++23 compiler found!"
        return 1
    fi
}

# Compile the native build system
compile_build_system() {
    log_info "Compiling XINIM native build system..."
    
    local compiler="clang++"
    if ! command -v clang++ >/dev/null 2>&1; then
        compiler="g++"
    fi
    
    # Use C++20 for maximum compatibility
    $compiler -std=c++20 -stdlib=libc++ -O2 \
        -Wall -Wextra -Wpedantic \
        -o "$BUILD_TOOL_BINARY" \
        "$BUILD_TOOL_SOURCE"
    
    log_success "Native build system compiled successfully"
}

# Main execution
main() {
    log_info "XINIM Native BSD Build System Bootstrap"
    log_info "======================================"
    
    cd "$SCRIPT_DIR"
    
    # Check prerequisites
    if ! check_compiler; then
        log_error "Cannot proceed without C++23 compiler"
        exit 1
    fi
    
    # Compile our native build system if needed
    if [[ ! -x "$BUILD_TOOL_BINARY" ]] || [[ "$BUILD_TOOL_SOURCE" -nt "$BUILD_TOOL_BINARY" ]]; then
        compile_build_system
    else
        log_info "Native build system is up to date"
    fi
    
    # Run the native build system
    log_info "Running XINIM native build system..."
    "$BUILD_TOOL_BINARY" "$@"
    
    local exit_code=$?
    if [[ $exit_code -eq 0 ]]; then
        log_success "Build completed successfully!"
    else
        log_error "Build failed with exit code $exit_code"
    fi
    
    return $exit_code
}

# Handle command line arguments
case "${1:-build}" in
    "clean")
        log_info "Cleaning build artifacts..."
        rm -rf build
        rm -f "$BUILD_TOOL_BINARY"
        log_success "Clean completed"
        ;;
    "rebuild")
        log_info "Rebuilding from scratch..."
        rm -rf build
        rm -f "$BUILD_TOOL_BINARY" 
        main
        ;;
    "build"|*)
        main "$@"
        ;;
esac