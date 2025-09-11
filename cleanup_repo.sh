#!/bin/bash
# XINIM Repository Cleanup Script
# Remove all legacy detritus and migrate to pure xmake build system

set -e

echo "================================================================================"
echo "XINIM REPOSITORY CLEANUP & XMAKE MIGRATION"
echo "================================================================================"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[CLEAN]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Function to safely remove files/directories
safe_remove() {
    local item="$1"
    if [[ -e "$item" ]]; then
        print_status "Removing: $item"
        rm -rf "$item"
    else
        print_warning "Not found: $item"
    fi
}

print_status "Starting comprehensive repository cleanup..."

# ============================================================================
# LEGACY BUILD SYSTEM FILES
# ============================================================================
print_status "Removing legacy build system files..."

# CMake files
safe_remove "CMakeLists.txt.backup_20250901_225528"
safe_remove "CMakePresets.json"
safe_remove "build/CMakeFiles"
safe_remove "build/CMakeCache.txt"
safe_remove "build/cmake_install.cmake"
safe_remove "build/Makefile"

# Bazel files
safe_remove "BUILD.bazel"
safe_remove "WORKSPACE.bazel"

# Legacy build scripts
safe_remove "build.sh"

# ============================================================================
# TEST EXECUTABLES AND ARTIFACTS
# ============================================================================
print_status "Removing test executables and artifacts..."

safe_remove "cat_test"
safe_remove "echo_test"
safe_remove "pwd_test"
safe_remove "test_stdlib"
safe_remove "xinim_build"
safe_remove "test_architecture_demo.cpp"

# ============================================================================
# DOWNLOADED ARCHIVES AND PACKAGES
# ============================================================================
print_status "Removing downloaded archives and packages..."

safe_remove "gxemul-0.7.0.tar.gz"
safe_remove "MacPorts-2.9.0-14-Sonoma.pkg"

# ============================================================================
# TEMPORARY AND BACKUP DIRECTORIES
# ============================================================================
print_status "Removing temporary and backup directories..."

safe_remove "audit_results"
safe_remove "audit_script.sh"
safe_remove "cleanup_backups"
safe_remove "harmony_workspace"
safe_remove "reconcile_duplicates.sh"

# ============================================================================
# BUILD ARTIFACTS
# ============================================================================
print_status "Cleaning build artifacts..."

safe_remove "build"
safe_remove ".xmake/build"
safe_remove "lib"
safe_remove "bin"  # We'll recreate this properly

# ============================================================================
# LEGACY DOCUMENTATION FILES (kept essential ones)
# ============================================================================
print_status "Removing redundant documentation files..."

# Keep only the main README.md, remove others
safe_remove "AGENTS.md"
safe_remove "ARCHITECTURE.md"
safe_remove "BUILD_REPORT.md"
safe_remove "BUILD_STATUS.md"
safe_remove "BUILD_SYSTEMS_COMPARISON.md"
safe_remove "MIGRATION_REPORT.md"
safe_remove "POSIX_CPP23_IMPLEMENTATION.md"
safe_remove "POSIX_TOOLS_ANALYSIS.md"
safe_remove "PROJECT_STATUS.md"
safe_remove "REPOSITORY_HYGIENE.md"

# ============================================================================
# VERIFY CLEANUP
# ============================================================================
print_status "Verifying cleanup..."

echo ""
echo "Remaining files in root directory:"
ls -la | grep -E "^-" | wc -l
echo "Remaining directories in root directory:"
ls -la | grep -E "^d" | wc -l

print_status "Cleanup complete! Repository is now clean and organized."

# ============================================================================
# ENSURE XMAKE CONFIGURATION
# ============================================================================
print_status "Ensuring xmake configuration is optimal..."

if [[ ! -f "xmake.lua" ]]; then
    print_error "xmake.lua not found! Restoring from backup..."
    if [[ -f "xmake_full.lua" ]]; then
        mv xmake_full.lua xmake.lua
        print_status "Restored xmake.lua from backup"
    else
        print_error "No xmake.lua backup found!"
        exit 1
    fi
fi

# Clean any remaining xmake artifacts
safe_remove ".xmake/build"
safe_remove ".xmake/cache"

print_status "xmake configuration verified!"

echo ""
echo "================================================================================"
echo "REPOSITORY CLEANUP COMPLETE!"
echo "================================================================================"
echo ""
echo "Next steps:"
echo "1. Run 'xmake config --toolchain=clang' to configure"
echo "2. Run 'xmake build' to build the project"
echo "3. Run 'xmake run xinim-test' to test"
echo ""
print_status "XINIM repository is now pure xmake with zero legacy detritus! 🎉"
