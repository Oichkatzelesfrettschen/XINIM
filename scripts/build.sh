#!/usr/bin/env bash
# Xinim CMake + Conan build wrapper

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[BUILD]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

MODE="debug"
CLEAN=false
TEST=false
DOCS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug) MODE="debug" ;;
        --release) MODE="release" ;;
        --clean) CLEAN=true ;;
        --test) TEST=true ;;
        --docs) DOCS=true ;;
        -h|--help)
            cat << EOF
Usage: $0 [--debug|--release] [--clean] [--test] [--docs]

Builds Xinim using CMake presets and Conan.
EOF
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
    shift
done

if [[ "$CLEAN" == true ]]; then
    print_status "Cleaning build artifacts..."
    rm -rf build
fi

print_status "Installing dependencies via Conan..."
PROFILE_ARG=""
if [[ -f conan/profiles/xinim-clang ]]; then
    PROFILE_ARG="conan/profiles/xinim-clang"
fi
./scripts/conan_install.sh . "${MODE^}" "$PROFILE_ARG"

print_status "Configuring CMake preset: $MODE"
cmake --preset "$MODE"

print_status "Building..."
cmake --build --preset "$MODE"

if [[ "$TEST" == true ]]; then
    print_status "Running tests..."
    ctest --output-on-failure --test-dir build
fi

if [[ "$DOCS" == true ]]; then
    print_status "Generating documentation..."
    cmake --build --preset "$MODE" --target docs
fi

print_status "Build completed successfully."
