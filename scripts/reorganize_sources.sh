#!/bin/bash
# XINIM POSIX/UNIX/BSD Source Structure Reorganization
# Move all source files to proper /src and /include locations

set -e

echo "================================================================================"
echo "XINIM POSIX/UNIX/BSD SOURCE STRUCTURE REORGANIZATION"
echo "================================================================================"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[REORG]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Create proper directory structure
print_status "Creating POSIX-compliant directory structure..."

mkdir -p src/{kernel,hal,crypto,fs,mm,net,commands,tools,common,boot}
mkdir -p include/{sys,c++,posix,xinim}
mkdir -p test/{unit,integration,posix}
mkdir -p docs/{api,guides,specs}

# Function to safely move files
safe_move() {
    local src="$1"
    local dst="$2"
    if [[ -e "$src" ]]; then
        print_status "Moving: $src -> $dst"
        mkdir -p "$(dirname "$dst")"
        mv "$src" "$dst"
    fi
}

# ============================================================================
# MOVE SOURCE FILES TO /src
# ============================================================================

print_status "Moving source files to /src directory..."

# Commands
if [[ -d "commands" ]]; then
    find commands -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#commands/}"
        safe_move "$file" "src/commands/$relative_path"
    done
fi

# Tools
if [[ -d "tools" ]]; then
    find tools -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#tools/}"
        safe_move "$file" "src/tools/$relative_path"
    done
fi

# Common
if [[ -d "common" ]]; then
    find common -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#common/}"
        safe_move "$file" "src/common/$relative_path"
    done
fi

# Boot
if [[ -d "boot" ]]; then
    find boot -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#boot/}"
        safe_move "$file" "src/boot/$relative_path"
    done
fi

# Kernel (if exists)
if [[ -d "kernel" ]]; then
    find kernel -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#kernel/}"
        safe_move "$file" "src/kernel/$relative_path"
    done
fi

# Crypto (if exists)
if [[ -d "crypto" ]]; then
    find crypto -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#crypto/}"
        safe_move "$file" "src/crypto/$relative_path"
    done
fi

# Filesystem (if exists)
if [[ -d "fs" ]]; then
    find fs -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#fs/}"
        safe_move "$file" "src/fs/$relative_path"
    done
fi

# Memory management (if exists)
if [[ -d "mm" ]]; then
    find mm -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#mm/}"
        safe_move "$file" "src/mm/$relative_path"
    done
fi

# ============================================================================
# MOVE HEADER FILES TO /include
# ============================================================================

print_status "Moving header files to /include directory..."

# H directory (system headers)
if [[ -d "h" ]]; then
    find h -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#h/}"
        safe_move "$file" "include/sys/$relative_path"
    done
fi

# ============================================================================
# MOVE TEST FILES TO /test
# ============================================================================

print_status "Moving test files to /test directory..."

# Tests directory
if [[ -d "tests" ]]; then
    find tests -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | while read -r file; do
        relative_path="${file#tests/}"
        safe_move "$file" "test/$relative_path"
    done
fi

# ============================================================================
# CLEAN UP EMPTY DIRECTORIES
# ============================================================================

print_status "Cleaning up empty directories..."

# Remove empty directories
find . -type d -empty -delete 2>/dev/null || true

# Remove specific empty directories we know should be gone
rmdir commands tools common boot kernel crypto fs mm h tests 2>/dev/null || true

# ============================================================================
# VERIFY REORGANIZATION
# ============================================================================

print_status "Verifying reorganization..."

echo ""
echo "Source files distribution after reorganization:"
find src -name "*.cpp" -o -name "*.hpp" -o -name "*.h" 2>/dev/null | wc -l || echo "0"
echo "Header files in include/:"
find include -name "*.cpp" -o -name "*.hpp" -o -name "*.h" 2>/dev/null | wc -l || echo "0"
echo "Test files in test/:"
find test -name "*.cpp" -o -name "*.hpp" -o -name "*.h" 2>/dev/null | wc -l || echo "0"

echo ""
echo "Remaining directories in root:"
ls -d */ 2>/dev/null | grep -v "^src/$" | grep -v "^include/$" | grep -v "^test/$" | grep -v "^docs/$" | grep -v "^scripts/$" | grep -v "^third_party/$" | grep -v "^.git/$" | grep -v "^.github/$" | grep -v "^.vscode/$" | grep -v "^.xmake/$" | wc -l

print_status "Source structure reorganization complete!"

echo ""
echo "================================================================================"
echo "POSIX/UNIX/BSD COMPLIANT STRUCTURE ACHIEVED!"
echo "================================================================================"
echo ""
echo "Directory structure:"
echo "├── src/           # All source code (.cpp)"
echo "├── include/       # All headers (.hpp/.h)"
echo "├── test/          # All test files"
echo "├── docs/          # Documentation"
echo "├── scripts/       # Build scripts"
echo "└── third_party/   # External dependencies"
echo ""
print_status "XINIM now has true POSIX/UNIX/BSD source organization! 🎉"
