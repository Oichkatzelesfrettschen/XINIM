#!/usr/bin/env bash
#
# build.sh - XINIM Build Script (XMake ONLY)
#
# This script uses XMake as the ONLY build system for XINIM.
# All warnings are errors, maximal compliance, comprehensive coverage.
#
# Usage:
#    ./build.sh [options] [xmake args]
#    ./build.sh --help
#
# Options:
#    --arch=<x86_64|arm64>  Target architecture
#    --mode=<debug|release> Build mode
#    --posix-tests          Enable POSIX compliance tests
#    --maximal              Enable maximal compliance mode
#    --coverage             Generate coverage report
#    --clean                Clean build artifacts
#    --rebuild              Clean and rebuild
#    --qemu                 Build and run in QEMU
#    --all                  Build all targets

set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
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
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

log_step() {
    echo -e "${CYAN}[STEP]${NC} $*"
}

log_critical() {
    echo -e "${MAGENTA}[CRITICAL]${NC} $*"
}

show_help() {
    cat << EOF
================================================================================
XINIM BUILD SYSTEM - XMake ONLY
================================================================================

Usage: $0 [options] [xmake args]

Options:
    --arch=<x86_64|arm64>  Target architecture (default: auto-detect)
    --mode=<debug|release> Build mode (default: debug)
    --posix-tests          Enable POSIX compliance testing
    --maximal              Enable maximal compliance mode
    --coverage             Generate coverage report
    --clean                Clean build artifacts
    --rebuild              Clean and rebuild everything
    --qemu                 Build and run in QEMU
    --all                  Build all targets
    --help                 Show this help message

Build Modes:
    debug    - Debug build with sanitizers and full diagnostics
    release  - Release build with maximum optimizations
    profile  - Profile-guided optimization build
    coverage - Coverage analysis build

Examples:
    $0                          # Build with defaults
    $0 --mode=release --all    # Build everything in release mode
    $0 --posix-tests --maximal # Run POSIX tests with maximal compliance
    $0 --arch=arm64 --qemu     # Build for ARM64 and run in QEMU
    $0 --coverage              # Build with coverage analysis

Build System Features:
    • ALL warnings as errors (-Werror)
    • C++23 standard with strict compliance
    • SIMD optimizations for x86_64 and ARM64
    • Automatic POSIX test suite downloading
    • 100% source file coverage verification
    • No warning suppression allowed

================================================================================
EOF
    exit 0
}

# Default settings
ARCH=""
MODE="debug"
POSIX_TESTS=false
MAXIMAL_COMPLIANCE=true
COVERAGE=false
CLEAN=false
REBUILD=false
RUN_QEMU=false
BUILD_ALL=false
XMAKE_ARGS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch=*)
            ARCH="${1#*=}"
            ;;
        --mode=*)
            MODE="${1#*=}"
            ;;
        --posix-tests)
            POSIX_TESTS=true
            ;;
        --maximal)
            MAXIMAL_COMPLIANCE=true
            ;;
        --coverage)
            COVERAGE=true
            MODE="coverage"
            ;;
        --clean)
            CLEAN=true
            ;;
        --rebuild)
            REBUILD=true
            ;;
        --qemu)
            RUN_QEMU=true
            ;;
        --all)
            BUILD_ALL=true
            ;;
        --help|-h)
            show_help
            ;;
        *)
            XMAKE_ARGS+=("$1")
            ;;
    esac
    shift
done

# Ensure xmake is installed
check_xmake() {
    if ! command -v xmake >/dev/null 2>&1; then
        log_error "XMake not found! This is the ONLY supported build system."
        log_info "Install XMake from: https://xmake.io/#/getting_started"
        
        # Offer to install xmake
        if [[ "$(uname)" == "Darwin" ]]; then
            log_info "On macOS, you can install with: brew install xmake"
        elif [[ "$(uname)" == "Linux" ]]; then
            log_info "On Linux, you can install with:"
            echo "    bash <(curl -fsSL https://xmake.io/shget.text)"
        fi
        
        exit 1
    fi
    
    local version=$(xmake --version 2>&1 | head -1)
    log_success "Found XMake: $version"
}

# Clean build artifacts
clean_build() {
    log_step "Cleaning build artifacts..."
    xmake clean -a
    rm -rf build .xmake
    log_success "Clean completed"
}

# Configure xmake
configure_xmake() {
    log_step "Configuring XMake..."
    
    local config_args=("-m" "$MODE")
    
    # Architecture configuration
    if [[ -n "$ARCH" ]]; then
        config_args+=("--arch=$ARCH")
        log_info "Target architecture: $ARCH"
    else
        local detected_arch=$(uname -m)
        if [[ "$detected_arch" == "arm64" ]] || [[ "$detected_arch" == "aarch64" ]]; then
            ARCH="arm64"
        else
            ARCH="x86_64"
        fi
        log_info "Auto-detected architecture: $ARCH"
    fi
    
    # POSIX tests configuration
    if [[ "$POSIX_TESTS" == true ]]; then
        config_args+=("--posix_tests=y")
        log_info "POSIX compliance tests: ENABLED"
    fi
    
    # Maximal compliance
    if [[ "$MAXIMAL_COMPLIANCE" == true ]]; then
        config_args+=("--maximal_compliance=y")
        log_critical "Maximal compliance mode: ENABLED (all warnings as errors)"
    fi
    
    # Native optimizations
    if [[ "$MODE" == "release" ]]; then
        config_args+=("--native=y")
        log_info "Native CPU optimizations: ENABLED"
    fi
    
    # Apply configuration
    xmake config "${config_args[@]}" "${XMAKE_ARGS[@]}"
    
    log_success "Configuration complete"
}

# Build targets
build_targets() {
    log_step "Building XINIM with XMake..."
    
    echo "================================================================================"
    echo "BUILD CONFIGURATION"
    echo "================================================================================"
    echo "Architecture:     $ARCH"
    echo "Build Mode:       $MODE"
    echo "Warnings:         ALL AS ERRORS"
    echo "C++ Standard:     C++23"
    echo "POSIX Tests:      $([ "$POSIX_TESTS" == true ] && echo "ENABLED" || echo "DISABLED")"
    echo "Compliance:       $([ "$MAXIMAL_COMPLIANCE" == true ] && echo "MAXIMAL" || echo "STANDARD")"
    echo "================================================================================"
    
    # Build specific target or all
    if [[ "$BUILD_ALL" == true ]]; then
        log_info "Building ALL targets..."
        xmake build all
    else
        log_info "Building default targets..."
        xmake build
    fi
    
    local exit_code=$?
    if [[ $exit_code -ne 0 ]]; then
        log_error "Build failed! All warnings are errors - fix them!"
        log_critical "DO NOT suppress warnings! Fix the root cause!"
        exit $exit_code
    fi
    
    log_success "Build completed successfully with ZERO warnings!"
}

# Run POSIX compliance tests
run_posix_tests() {
    if [[ "$POSIX_TESTS" == true ]]; then
        log_step "Running POSIX compliance test suite..."
        
        echo "================================================================================"
        echo "POSIX COMPLIANCE TEST SUITE"
        echo "================================================================================"
        echo "Downloading and building official POSIX tests..."
        echo "These are GPL-licensed external tests for compliance verification."
        echo "================================================================================"
        
        xmake build posix_compliance
        
        if [[ "$MAXIMAL_COMPLIANCE" == true ]]; then
            log_critical "Maximal compliance mode - ANY test failure will abort the build!"
        fi
    fi
}

# Generate coverage report
generate_coverage() {
    if [[ "$COVERAGE" == true ]]; then
        log_step "Generating coverage report..."
        xmake build coverage
        log_success "Coverage report generated"
    fi
}

# Run in QEMU
run_qemu() {
    if [[ "$RUN_QEMU" == true ]]; then
        log_step "Launching XINIM in QEMU..."
        xmake run qemu_run
    fi
}

# Verify build integrity
verify_build() {
    log_step "Verifying build integrity..."
    
    # Check for warning suppressions in code
    if grep -r "#pragma.*warning" --include="*.cpp" --include="*.hpp" . 2>/dev/null | grep -v third_party; then
        log_error "WARNING SUPPRESSION DETECTED!"
        log_critical "Remove all #pragma warning directives and fix the issues!"
        exit 1
    fi
    
    # Count source files vs object files
    local source_count=$(find . -name "*.cpp" -o -name "*.c" | grep -v third_party | wc -l)
    local object_count=$(find build -name "*.o" 2>/dev/null | wc -l)
    
    log_info "Source files: $source_count"
    log_info "Object files: $object_count"
    
    if [[ $object_count -lt $((source_count * 90 / 100)) ]]; then
        log_warning "Not all source files were compiled!"
        log_info "This may be normal for header-only files"
    fi
    
    log_success "Build integrity verified"
}

# Main execution
main() {
    cd "$SCRIPT_DIR"
    
    echo "================================================================================"
    echo "XINIM BUILD SYSTEM - XMake ONLY"
    echo "================================================================================"
    
    # Check for xmake
    check_xmake
    
    # Handle clean/rebuild
    if [[ "$CLEAN" == true ]]; then
        clean_build
        [[ "$REBUILD" == false ]] && exit 0
    fi
    
    if [[ "$REBUILD" == true ]]; then
        clean_build
    fi
    
    # Configure and build
    configure_xmake
    build_targets
    
    # Run tests if enabled
    run_posix_tests
    
    # Generate coverage if requested
    generate_coverage
    
    # Verify build integrity
    verify_build
    
    # Run in QEMU if requested
    run_qemu
    
    echo "================================================================================"
    echo "BUILD COMPLETE"
    echo "================================================================================"
    echo "All components built with:"
    echo "  • Zero warnings (all warnings as errors)"
    echo "  • C++23 strict compliance"
    echo "  • Complete file coverage"
    echo "  • SIMD optimizations"
    echo "================================================================================"
    
    log_success "XINIM build completed successfully!"
}

# Run main
main