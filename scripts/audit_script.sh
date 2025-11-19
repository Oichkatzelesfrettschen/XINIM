#!/bin/bash
# XINIM Project Audit and Harmonization Script
# Comprehensive analysis and restructuring for C++23 POSIX compliance

set -e

echo "================================================================================"
echo "XINIM AUDIT & HARMONIZATION SCRIPT"
echo "================================================================================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Create audit directories
AUDIT_DIR="audit_results"
mkdir -p "$AUDIT_DIR"

print_status "Starting comprehensive audit..."

# 1. File structure analysis
print_status "Analyzing file structure..."
find . -type f -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | sort > "$AUDIT_DIR/source_files.txt"
find . -type f -name "*.md" | sort > "$AUDIT_DIR/markdown_files.txt"
find . -name "*.h" | wc -l > "$AUDIT_DIR/header_count.txt"
find . -name "*.hpp" | wc -l >> "$AUDIT_DIR/header_count.txt"

# 2. Directory structure analysis
print_status "Analyzing directory structure..."
tree -d -L 4 > "$AUDIT_DIR/directory_structure.txt" 2>/dev/null || find . -type d | sort > "$AUDIT_DIR/directory_structure.txt"

# 3. Build system analysis
print_status "Analyzing build systems..."
ls -la | grep -E "(CMakeLists|Makefile|xmake|meson|BUILD)" > "$AUDIT_DIR/build_systems.txt"

# 4. Header file analysis
print_status "Analyzing header files..."
find . -name "*.h" -exec grep -l "#include" {} \; | head -20 > "$AUDIT_DIR/c_headers.txt"
find . -name "*.hpp" -exec grep -l "#include" {} \; | head -20 > "$AUDIT_DIR/cpp_headers.txt"

print_status "Audit complete! Results saved in $AUDIT_DIR/"
echo "================================================================================"
