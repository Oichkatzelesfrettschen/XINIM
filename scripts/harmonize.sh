#!/usr/bin/env bash
# Xinim harmonization script (CMake + Conan)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# shellcheck disable=SC1091
export XINIM_REPO_ROOT="${PROJECT_ROOT}"
source "${SCRIPT_DIR}/xinim-env.sh"
xinim_ensure_project_dirs

cd "$PROJECT_ROOT"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[HARMONIZE]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

MODE="debug"
RUN_TESTS=false
RUN_DOCS=false
RUN_LINT=false
RUN_FORMAT=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug) MODE="debug" ;;
        --release) MODE="release" ;;
        --test) RUN_TESTS=true ;;
        --docs) RUN_DOCS=true ;;
        --lint) RUN_LINT=true ;;
        --format) RUN_FORMAT=true ;;
        -h|--help)
            cat << EOF
Usage: $0 [--debug|--release] [--test] [--docs] [--lint] [--format]

Runs standard build + optional QA tasks using CMake + Conan.
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

print_status "Installing dependencies via Conan..."
PROFILE_ARG=""
if [[ -f conan/profiles/xinim-clang ]]; then
    PROFILE_ARG="conan/profiles/xinim-clang"
fi
BUILD_DIR="${XINIM_BUILD_ROOT}/${MODE^}"
./scripts/conan_install.sh "${BUILD_DIR}" "${MODE^}" "$PROFILE_ARG"

print_status "Configuring build tree: ${BUILD_DIR}"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE="${MODE^}" \
    -DCMAKE_TOOLCHAIN_FILE="${BUILD_DIR}/generators/conan_toolchain.cmake" \
    -DXINIM_STATE_ROOT="${XINIM_STATE_ROOT}"

print_status "Building..."
cmake --build "${BUILD_DIR}"

if [[ "$RUN_FORMAT" == true ]]; then
    print_status "Formatting modified C++ files..."
    git diff --name-only --diff-filter=ACM | grep -E '\.(cpp|hpp|cc|hh|cxx|hxx)$' | \
        xargs -r clang-format -i
fi

if [[ "$RUN_LINT" == true ]]; then
    print_status "Running clang-tidy on modified C++ files..."
    git diff --name-only --diff-filter=ACM | grep -E '\.(cpp|hpp|cc|hh|cxx|hxx)$' | \
        xargs -r clang-tidy -p "${BUILD_DIR}"
fi

if [[ "$RUN_TESTS" == true ]]; then
    print_status "Running tests..."
    ctest --output-on-failure --test-dir "${BUILD_DIR}"
fi

if [[ "$RUN_DOCS" == true ]]; then
    print_status "Generating documentation..."
    cmake --build "${BUILD_DIR}" --target xinim_docs
fi

print_status "Harmonization complete."
