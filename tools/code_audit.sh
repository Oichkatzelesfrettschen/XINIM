#!/bin/bash

# XINIM Code Quality Audit Tool
# Identifies and reports code quality issues

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Counters
TOTAL_FILES=0
ISSUES_FOUND=0
FIXED_FILES=0

echo "========================================"
echo "XINIM Code Quality Audit"
echo "========================================"
echo ""

# Function to check file for issues
check_file() {
    local file="$1"
    local issues=""
    local has_issues=false
    
    # Check for K&R C style function definitions
    if grep -q '^[a-zA-Z_][a-zA-Z0-9_]*([^)]*)[[:space:]]*$' "$file" 2>/dev/null; then
        issues="${issues}  - K&R style function definition\n"
        has_issues=true
    fi
    
    # Check for missing header guards in .h/.hpp files
    if [[ "$file" =~ \.(h|hpp)$ ]]; then
        if ! grep -q "#ifndef.*_H\|#pragma once" "$file" 2>/dev/null; then
            issues="${issues}  - Missing header guard\n"
            has_issues=true
        fi
    fi
    
    # Check for PUBLIC/PRIVATE macros (old MINIX style)
    if grep -q "^PUBLIC\|^PRIVATE" "$file" 2>/dev/null; then
        issues="${issues}  - Old MINIX PUBLIC/PRIVATE macros\n"
        has_issues=true
    fi
    
    # Check for tabs vs spaces inconsistency
    if grep -q $'\t' "$file" 2>/dev/null && grep -q '^    ' "$file" 2>/dev/null; then
        issues="${issues}  - Mixed tabs and spaces\n"
        has_issues=true
    fi
    
    # Check for excessive line length
    if grep -E '^.{121,}$' "$file" 2>/dev/null | head -1 > /dev/null; then
        issues="${issues}  - Lines exceeding 120 characters\n"
        has_issues=true
    fi
    
    # Check for TODO/FIXME comments
    if grep -i "TODO\|FIXME\|XXX\|HACK" "$file" 2>/dev/null | head -1 > /dev/null; then
        issues="${issues}  - Contains TODO/FIXME comments\n"
        has_issues=true
    fi
    
    if [ "$has_issues" = true ]; then
        echo -e "${YELLOW}Issues in: $(basename "$file")${NC}"
        echo -e "$issues"
        ((ISSUES_FOUND++))
    fi
}

# Audit C/C++ source files
echo "🔍 Auditing C/C++ Source Files..."
echo "----------------------------------------"

while IFS= read -r -d '' file; do
    ((TOTAL_FILES++))
    check_file "$file"
done < <(find "$PROJECT_ROOT" \
    -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.hpp" -o -name "*.h" \) \
    -not -path "*/build/*" \
    -not -path "*/third_party/*" \
    -not -path "*/cleanup_backups/*" \
    -not -path "*/.git/*" \
    -not -path "*/.xmake/*" \
    -print0)

echo ""
echo "🔍 Checking for Naming Issues..."
echo "----------------------------------------"

# Check for files with spaces in names
SPACE_FILES=$(find "$PROJECT_ROOT" -name "* *" -type f 2>/dev/null | grep -v ".git\|build\|cleanup_backups" | wc -l)
if [ "$SPACE_FILES" -gt 0 ]; then
    echo -e "${YELLOW}Files with spaces in names: $SPACE_FILES${NC}"
    find "$PROJECT_ROOT" -name "* *" -type f 2>/dev/null | grep -v ".git\|build\|cleanup_backups" | head -5
    ((ISSUES_FOUND+=$SPACE_FILES))
fi

# Check for duplicate pattern files
DUP_FILES=$(find "$PROJECT_ROOT" -name "* 2.*" -type f 2>/dev/null | grep -v ".git\|build\|cleanup_backups" | wc -l)
if [ "$DUP_FILES" -gt 0 ]; then
    echo -e "${YELLOW}Duplicate pattern files: $DUP_FILES${NC}"
    find "$PROJECT_ROOT" -name "* 2.*" -type f 2>/dev/null | grep -v ".git\|build\|cleanup_backups" | head -5
    ((ISSUES_FOUND+=$DUP_FILES))
fi

echo ""
echo "🔍 Checking Build System..."
echo "----------------------------------------"

# Check for multiple build systems
BUILD_SYSTEMS=0
[ -f "$PROJECT_ROOT/CMakeLists.txt" ] && ((BUILD_SYSTEMS++)) && echo "  ✓ CMake found"
[ -f "$PROJECT_ROOT/xmake.lua" ] && ((BUILD_SYSTEMS++)) && echo "  ✓ xmake found"
[ -f "$PROJECT_ROOT/Makefile" ] && ((BUILD_SYSTEMS++)) && echo "  ✓ Makefile found"
[ -f "$PROJECT_ROOT/BUILD.bazel" ] && ((BUILD_SYSTEMS++)) && echo "  ✓ Bazel found"

if [ "$BUILD_SYSTEMS" -gt 1 ]; then
    echo -e "${YELLOW}Warning: Multiple build systems detected${NC}"
fi

echo ""
echo "🔍 License Compliance Check..."
echo "----------------------------------------"

# Check for GPL contamination outside third_party
GPL_FILES=$(grep -r "GPL\|GNU General Public" "$PROJECT_ROOT" \
    --exclude-dir=third_party \
    --exclude-dir=.git \
    --exclude-dir=build \
    --exclude-dir=cleanup_backups \
    --include="*.c" --include="*.cpp" --include="*.h" --include="*.hpp" \
    2>/dev/null | wc -l)

if [ "$GPL_FILES" -gt 0 ]; then
    echo -e "${RED}GPL references found outside third_party: $GPL_FILES${NC}"
else
    echo -e "${GREEN}✓ No GPL contamination detected${NC}"
fi

echo ""
echo "========================================"
echo "AUDIT SUMMARY"
echo "========================================"
echo "Total files scanned: $TOTAL_FILES"
echo "Issues found: $ISSUES_FOUND"

if [ "$ISSUES_FOUND" -eq 0 ]; then
    echo -e "${GREEN}✓ No major issues found!${NC}"
else
    echo -e "${YELLOW}⚠ $ISSUES_FOUND issues need attention${NC}"
fi

echo ""
echo "Recommendations:"
echo "  1. Fix K&R style functions with modern C++ syntax"
echo "  2. Add header guards to all header files"
echo "  3. Replace PUBLIC/PRIVATE macros with proper C++ visibility"
echo "  4. Standardize on spaces (4 spaces recommended)"
echo "  5. Consider addressing TODO/FIXME comments"
echo ""