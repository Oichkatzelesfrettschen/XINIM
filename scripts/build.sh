#!/usr/bin/env bash
# Xinim CMake + Conan build wrapper

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# shellcheck disable=SC1091
export XINIM_REPO_ROOT="${PROJECT_ROOT}"
source "${SCRIPT_DIR}/xinim-env.sh"
xinim_ensure_project_dirs

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
LIST_LANES=false
LAUNCH=false
ALL_X86_32=false
LANE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug) MODE="debug" ;;
        --release) MODE="release" ;;
        --clean) CLEAN=true ;;
        --test) TEST=true ;;
        --docs) DOCS=true ;;
        --list-lanes) LIST_LANES=true ;;
        --launch) LAUNCH=true ;;
        --all-x86-32) ALL_X86_32=true ;;
        --lane)
            LANE="${2:-}"
            if [[ -z "${LANE}" ]]; then
                print_error "--lane requires a lane name"
                exit 1
            fi
            shift
            ;;
        -h|--help)
            cat << EOF
Usage: $0 [--debug|--release] [--clean] [--test] [--docs]
          [--list-lanes] [--lane NAME] [--all-x86-32] [--launch]

Builds Xinim using CMake + Conan with a repo-local state root by default.

Examples:
  $0 --debug --test
  $0 --debug --list-lanes
  $0 --debug --lane i586 --test
  $0 --debug --all-x86-32 --test
  $0 --debug --lane x86_64 --launch
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

if [[ -n "${LANE}" && "${ALL_X86_32}" == true ]]; then
    print_error "--lane and --all-x86-32 are mutually exclusive"
    exit 1
fi

if [[ "${LAUNCH}" == true && "${ALL_X86_32}" == true ]]; then
    print_error "--launch only supports one lane at a time"
    exit 1
fi

if [[ "${LAUNCH}" == true && -z "${LANE}" ]]; then
    print_error "--launch requires --lane NAME"
    exit 1
fi

if [[ "$CLEAN" == true ]]; then
    print_status "Cleaning build artifacts..."
    rm -rf "${XINIM_BUILD_ROOT}/${MODE^}"
fi

print_status "Installing dependencies via Conan..."
PROFILE_ARG=""
if [[ -f conan/profiles/xinim-clang ]]; then
    PROFILE_ARG="conan/profiles/xinim-clang"
fi
BUILD_DIR="${XINIM_BUILD_ROOT}/${MODE^}"
./scripts/conan_install.sh "${BUILD_DIR}" "${MODE^}" "$PROFILE_ARG"

print_status "Configuring CMake build tree: ${BUILD_DIR}"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE="${MODE^}" \
    -DCMAKE_TOOLCHAIN_FILE="${BUILD_DIR}/generators/conan_toolchain.cmake" \
    -DXINIM_STATE_ROOT="${XINIM_STATE_ROOT}"

if [[ "${LIST_LANES}" == true ]]; then
    print_status "Registered x86 lanes:"
    python3 "${PROJECT_ROOT}/scripts/qemu_matrix.py" --build-dir "${BUILD_DIR}" --list-lanes
    if [[ -z "${LANE}" && "${ALL_X86_32}" == false && "${TEST}" == false && "${DOCS}" == false && "${LAUNCH}" == false ]]; then
        print_status "Build completed successfully."
        exit 0
    fi
fi

if [[ -n "${LANE}" || "${ALL_X86_32}" == true ]]; then
    MATRIX_ARGS=(python3 "${PROJECT_ROOT}/scripts/qemu_matrix.py" --build-dir "${BUILD_DIR}")
    if [[ "${ALL_X86_32}" == true ]]; then
        MATRIX_ARGS+=(--all-x86-32)
    else
        MATRIX_ARGS+=(--lane "${LANE}")
    fi

    print_status "Preparing lane image targets..."
    "${MATRIX_ARGS[@]}" --prepare

    if [[ "${TEST}" == true ]]; then
        print_status "Running lane tests..."
        "${MATRIX_ARGS[@]}" --test all
    fi

    if [[ "${LAUNCH}" == true ]]; then
        print_status "Launching selected lane..."
        "${MATRIX_ARGS[@]}" --launch
    fi
else
    print_status "Building..."
    cmake --build "${BUILD_DIR}"

    if [[ "$TEST" == true ]]; then
        print_status "Running tests..."
        ctest --output-on-failure --test-dir "${BUILD_DIR}"
    fi
fi


if [[ "$DOCS" == true ]]; then
    print_status "Generating documentation..."
    cmake --build "${BUILD_DIR}" --target xinim_docs
fi

print_status "Build completed successfully."
