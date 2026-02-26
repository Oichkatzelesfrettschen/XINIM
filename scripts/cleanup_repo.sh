#!/usr/bin/env bash
# Cleanup script for CMake + Conan artifacts

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[CLEAN]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

safe_remove() {
    if [[ -e "$1" ]]; then
        print_status "Removing $1"
        rm -rf "$1"
    fi
}

safe_remove build
safe_remove build_debug
safe_remove dist
safe_remove docs/doxygen
safe_remove docs/sphinx/html
safe_remove compile_commands.json
safe_remove cppcheck_report.txt

print_status "Cleanup complete."
