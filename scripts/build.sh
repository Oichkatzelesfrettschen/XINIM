#!/bin/bash
# XINIM Build Automation Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[BUILD]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check for xmake
if ! command -v xmake &> /dev/null; then
    print_error "xmake not found. Installing..."
    curl -fsSL https://xmake.io/shget.text | bash
    source ~/.xmake/profile
fi

# Parse arguments
MODE="release"
TOOLCHAIN="clang"
CLEAN=false
TEST=false
DOCS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug) MODE="debug" ;;
        --release) MODE="release" ;;
        --profile) MODE="profile" ;;
        --coverage) MODE="coverage" ;;
        --gcc) TOOLCHAIN="gcc" ;;
        --clang) TOOLCHAIN="clang" ;;
        --clean) CLEAN=true ;;
        --test) TEST=true ;;
        --docs) DOCS=true ;;
        *) print_error "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

print_status "Building XINIM with xmake"
print_status "Mode: $MODE"
print_status "Toolchain: $TOOLCHAIN"

# Clean if requested
if [[ "$CLEAN" == true ]]; then
    print_status "Cleaning build artifacts..."
    xmake clean
fi

# Configure
print_status "Configuring build..."
xmake config --mode="$MODE" --toolchain="$TOOLCHAIN"

# Build
print_status "Building XINIM..."
xmake build --verbose

# Test if requested
if [[ "$TEST" == true ]]; then
    print_status "Running tests..."
    xmake run test-unit
    xmake run test-integration
    xmake run test-posix
fi

# Generate docs if requested
if [[ "$DOCS" == true ]]; then
    print_status "Generating documentation..."
    xmake docs
fi

print_status "Build completed successfully! 🎉"
print_status "Binary location: $(xmake show --files)"
