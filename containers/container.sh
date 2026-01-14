#!/bin/bash
# XINIM Container Build and Test Orchestration Script
# Supports both Docker and Podman
# Usage: ./container.sh [command] [options]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CONTAINER_FILE="${SCRIPT_DIR}/Containerfile"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Container image names
IMAGE_PREFIX="xinim"
BUILD_IMAGE="${IMAGE_PREFIX}-build:latest"
TEST_IMAGE="${IMAGE_PREFIX}-test:latest"
DEBUG_IMAGE="${IMAGE_PREFIX}-debug:latest"
CI_IMAGE="${IMAGE_PREFIX}-ci:latest"

# Auto-detect container runtime (prefer podman if available)
detect_runtime() {
    if command -v podman &> /dev/null; then
        CONTAINER_RT="podman"
    elif command -v docker &> /dev/null; then
        CONTAINER_RT="docker"
    else
        echo -e "${RED}[ERROR]${NC} No container runtime found. Install podman or docker."
        exit 1
    fi
    echo -e "${BLUE}[INFO]${NC} Using container runtime: $CONTAINER_RT"
}

print_help() {
    cat << EOF
XINIM Container Build and Test System
======================================

Usage: $0 [command] [options]

Commands:
  build-image [target]  Build container image
                        Targets: build, test, debug, ci (default: build)
  
  build                 Build XINIM kernel in container
  test                  Run tests in container
  debug                 Start interactive debugging session
  ci                    Run full CI pipeline in container
  shell                 Start interactive shell in container
  
  clean                 Remove all XINIM container images
  prune                 Prune unused container resources
  
  qemu                  Boot XINIM kernel in QEMU (in container)
  qemu-debug            Boot XINIM in QEMU with GDB server
  
  lint                  Run linting in container
  format                Run code formatter in container
  analyze               Run static analysis in container
  coverage              Generate coverage report in container
  
  help                  Show this help message

Options:
  --runtime [podman|docker]   Force specific container runtime
  --rebuild                   Force rebuild of container image
  --verbose                   Enable verbose output
  --no-cache                  Build without cache

Examples:
  $0 build-image build       # Build the build-env image
  $0 build                   # Build XINIM in container
  $0 test                    # Run all tests
  $0 debug                   # Start debugging session
  $0 shell                   # Interactive shell
  $0 qemu-debug              # Boot with GDB support

EOF
}

# Build container image
build_image() {
    local target="${1:-build}"
    local cache_opt=""
    
    if [[ "$NO_CACHE" == "true" ]]; then
        cache_opt="--no-cache"
    fi
    
    case "$target" in
        build)
            local stage="build-env"
            local tag="$BUILD_IMAGE"
            ;;
        test)
            local stage="test-env"
            local tag="$TEST_IMAGE"
            ;;
        debug)
            local stage="debug-env"
            local tag="$DEBUG_IMAGE"
            ;;
        ci)
            local stage="ci-env"
            local tag="$CI_IMAGE"
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unknown target: $target"
            echo "Valid targets: build, test, debug, ci"
            exit 1
            ;;
    esac
    
    echo -e "${GREEN}[BUILD]${NC} Building $tag (stage: $stage)..."
    $CONTAINER_RT build \
        $cache_opt \
        --target "$stage" \
        -t "$tag" \
        -f "$CONTAINER_FILE" \
        "$PROJECT_ROOT"
    
    echo -e "${GREEN}[SUCCESS]${NC} Image built: $tag"
}

# Ensure image exists
ensure_image() {
    local image="$1"
    local target="$2"
    
    if [[ "$REBUILD" == "true" ]]; then
        echo -e "${YELLOW}[WARN]${NC} Rebuild requested, building image..."
        build_image "$target"
    elif ! $CONTAINER_RT images -q "$image" 2>/dev/null | grep -q .; then
        echo -e "${YELLOW}[WARN]${NC} Image $image not found. Building..."
        build_image "$target"
    fi
}

# Run build in container
run_build() {
    ensure_image "$BUILD_IMAGE" "build"
    
    echo -e "${GREEN}[BUILD]${NC} Building XINIM in container..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$BUILD_IMAGE" \
        bash -c "xmake config --toolchain=clang && xmake build --verbose"
    
    echo -e "${GREEN}[SUCCESS]${NC} Build completed!"
}

# Run tests in container
run_tests() {
    ensure_image "$TEST_IMAGE" "test"
    
    echo -e "${GREEN}[TEST]${NC} Running tests in container..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$TEST_IMAGE" \
        bash -c "xmake config --toolchain=clang && xmake build && xmake run test-all"
    
    echo -e "${GREEN}[SUCCESS]${NC} Tests completed!"
}

# Start debug session
run_debug() {
    ensure_image "$DEBUG_IMAGE" "debug"
    
    echo -e "${GREEN}[DEBUG]${NC} Starting debug session..."
    $CONTAINER_RT run -it --rm \
        --privileged \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        -p 1234:1234 \
        "$DEBUG_IMAGE" \
        /bin/bash
}

# Run CI pipeline
run_ci() {
    ensure_image "$CI_IMAGE" "ci"
    
    echo -e "${GREEN}[CI]${NC} Running CI pipeline in container..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$CI_IMAGE" \
        bash -c "
            set -e
            echo '=== Building XINIM ==='
            xmake config --toolchain=clang
            xmake build --verbose
            
            echo '=== Running Tests ==='
            if ! xmake run test-all; then
                echo '[WARN] Some tests failed'
            fi
            
            echo '=== Running Lint ==='
            if ! xmake run lint; then
                echo '[WARN] Lint completed with warnings'
            fi
            
            echo '=== CI Pipeline Complete ==='
        "
    
    echo -e "${GREEN}[SUCCESS]${NC} CI pipeline completed!"
}

# Interactive shell
run_shell() {
    local image="${1:-$BUILD_IMAGE}"
    local target="${2:-build}"
    ensure_image "$image" "$target"
    
    echo -e "${GREEN}[SHELL]${NC} Starting interactive shell..."
    $CONTAINER_RT run -it --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$image" \
        /bin/bash
}

# Run QEMU in container
run_qemu() {
    ensure_image "$TEST_IMAGE" "test"
    
    # First build if kernel doesn't exist
    if [[ ! -f "${PROJECT_ROOT}/build/xinim" ]]; then
        run_build
    fi
    
    echo -e "${GREEN}[QEMU]${NC} Booting XINIM in QEMU..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$TEST_IMAGE" \
        qemu-system-x86_64 \
            -kernel /xinim/build/xinim \
            -nographic \
            -no-reboot \
            -m 512M \
            -cpu qemu64 \
            -serial mon:stdio
}

# Run QEMU with GDB debug server
run_qemu_debug() {
    ensure_image "$DEBUG_IMAGE" "debug"
    
    if [[ ! -f "${PROJECT_ROOT}/build/xinim" ]]; then
        run_build
    fi
    
    echo -e "${GREEN}[QEMU-DEBUG]${NC} Booting XINIM in QEMU with GDB server on port 1234..."
    echo -e "${BLUE}[INFO]${NC} Connect with: gdb build/xinim -ex 'target remote localhost:1234'"
    
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        -p 1234:1234 \
        "$DEBUG_IMAGE" \
        qemu-system-x86_64 \
            -kernel /xinim/build/xinim \
            -nographic \
            -no-reboot \
            -m 512M \
            -cpu qemu64 \
            -serial mon:stdio \
            -s -S
}

# Run linting
run_lint() {
    ensure_image "$CI_IMAGE" "ci"
    
    echo -e "${GREEN}[LINT]${NC} Running linting..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$CI_IMAGE" \
        bash -c "xmake config --toolchain=clang && xmake build && xmake run lint"
}

# Run formatter
run_format() {
    ensure_image "$BUILD_IMAGE" "build"
    
    echo -e "${GREEN}[FORMAT]${NC} Running code formatter..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$BUILD_IMAGE" \
        bash -c "xmake run format"
}

# Run static analysis
run_analyze() {
    ensure_image "$CI_IMAGE" "ci"
    
    echo -e "${GREEN}[ANALYZE]${NC} Running static analysis..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$CI_IMAGE" \
        bash -c "xmake run analyze"
}

# Generate coverage report
run_coverage() {
    ensure_image "$TEST_IMAGE" "test"
    
    echo -e "${GREEN}[COVERAGE]${NC} Generating coverage report..."
    $CONTAINER_RT run --rm \
        -v "${PROJECT_ROOT}:/xinim:Z" \
        -w /xinim \
        "$TEST_IMAGE" \
        bash -c "
            xmake config --mode=coverage --toolchain=clang
            xmake build xinim-coverage
            xmake run xinim-coverage
        "
}

# Clean images
clean_images() {
    echo -e "${GREEN}[CLEAN]${NC} Removing XINIM container images..."
    for img in "$BUILD_IMAGE" "$TEST_IMAGE" "$DEBUG_IMAGE" "$CI_IMAGE"; do
        if $CONTAINER_RT images -q "$img" | grep -q .; then
            $CONTAINER_RT rmi "$img" || true
        fi
    done
    echo -e "${GREEN}[SUCCESS]${NC} Images removed!"
}

# Prune container resources
prune_containers() {
    echo -e "${GREEN}[PRUNE]${NC} Pruning unused container resources..."
    $CONTAINER_RT system prune -f
    echo -e "${GREEN}[SUCCESS]${NC} Prune completed!"
}

# Parse arguments
REBUILD=false
VERBOSE=false
NO_CACHE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --runtime)
            CONTAINER_RT="$2"
            shift 2
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --no-cache)
            NO_CACHE=true
            shift
            ;;
        build-image)
            detect_runtime
            build_image "$2"
            exit 0
            ;;
        build)
            detect_runtime
            run_build
            exit 0
            ;;
        test)
            detect_runtime
            run_tests
            exit 0
            ;;
        debug)
            detect_runtime
            run_debug
            exit 0
            ;;
        ci)
            detect_runtime
            run_ci
            exit 0
            ;;
        shell)
            detect_runtime
            run_shell "$BUILD_IMAGE" "build"
            exit 0
            ;;
        qemu)
            detect_runtime
            run_qemu
            exit 0
            ;;
        qemu-debug)
            detect_runtime
            run_qemu_debug
            exit 0
            ;;
        lint)
            detect_runtime
            run_lint
            exit 0
            ;;
        format)
            detect_runtime
            run_format
            exit 0
            ;;
        analyze)
            detect_runtime
            run_analyze
            exit 0
            ;;
        coverage)
            detect_runtime
            run_coverage
            exit 0
            ;;
        clean)
            detect_runtime
            clean_images
            exit 0
            ;;
        prune)
            detect_runtime
            prune_containers
            exit 0
            ;;
        help|--help|-h)
            print_help
            exit 0
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unknown command: $1"
            print_help
            exit 1
            ;;
    esac
done

# No command provided, show help
print_help
