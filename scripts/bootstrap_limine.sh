#!/usr/bin/env bash
# Fetch and build the Limine binary release into the repo-local tools cache.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# shellcheck disable=SC1091
export XINIM_REPO_ROOT="${PROJECT_ROOT}"
source "${SCRIPT_DIR}/xinim-env.sh"
xinim_ensure_project_dirs

LIMINE_VERSION="${XINIM_LIMINE_VERSION:-v10.8.3-binary}"
LIMINE_REPO="${XINIM_LIMINE_REPO:-https://github.com/limine-bootloader/limine.git}"
LIMINE_ROOT="${XINIM_LIMINE_ROOT:-${XINIM_TOOLS_ROOT}/limine/${LIMINE_VERSION}}"
FORCE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            LIMINE_VERSION="${2:?missing value for --version}"
            LIMINE_ROOT="${XINIM_TOOLS_ROOT}/limine/${LIMINE_VERSION}"
            shift 2
            ;;
        --repo)
            LIMINE_REPO="${2:?missing value for --repo}"
            shift 2
            ;;
        --dest)
            LIMINE_ROOT="${2:?missing value for --dest}"
            shift 2
            ;;
        --force)
            FORCE=true
            shift
            ;;
        -h|--help)
            cat <<EOF
Usage: $0 [--version TAG] [--repo URL] [--dest PATH] [--force]

Bootstraps a Limine binary release into the repo-local tools cache so image
generation can remain offline after the initial seed step.
EOF
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

for tool in git make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool" >&2
        exit 1
    fi
done

if [[ "${FORCE}" == true ]]; then
    rm -rf "${LIMINE_ROOT}"
fi

if [[ ! -d "${LIMINE_ROOT}/.git" ]]; then
    mkdir -p "$(dirname "${LIMINE_ROOT}")"
    git clone "${LIMINE_REPO}" --branch "${LIMINE_VERSION}" --depth 1 "${LIMINE_ROOT}"
fi

make -C "${LIMINE_ROOT}"

required_files=(
    "${LIMINE_ROOT}/limine"
    "${LIMINE_ROOT}/limine-bios.sys"
    "${LIMINE_ROOT}/limine-bios-cd.bin"
    "${LIMINE_ROOT}/limine-uefi-cd.bin"
    "${LIMINE_ROOT}/BOOTX64.EFI"
)

for path in "${required_files[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "Missing expected Limine asset: ${path}" >&2
        exit 1
    fi
done

printf 'Limine bootstrap complete: %s\n' "${LIMINE_ROOT}"
