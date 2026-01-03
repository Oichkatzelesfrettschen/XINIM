#!/bin/bash
# XINIM Comprehensive Code Analysis Runner
# Runs all static analysis, security, and code quality tools

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${REPO_ROOT}/docs/analysis/reports"
CODE_METRICS="${OUTPUT_DIR}/code_metrics"
STATIC_ANALYSIS="${OUTPUT_DIR}/static_analysis"
TOOLS_MISC="${OUTPUT_DIR}/tools_misc"

echo -e "${GREEN}XINIM Comprehensive Code Analysis${NC}"
echo "======================================"
echo "Repository: ${REPO_ROOT}"
echo "Output: ${OUTPUT_DIR}"
echo ""

# Create output directories
mkdir -p "${CODE_METRICS}"
mkdir -p "${STATIC_ANALYSIS}"
mkdir -p "${TOOLS_MISC}"

# Function to run a command with status reporting
run_tool() {
    local tool_name="$1"
    local command="$2"
    
    echo -ne "${YELLOW}Running ${tool_name}...${NC} "
    if eval "${command}" > /dev/null 2>&1; then
        echo -e "${GREEN}✓${NC}"
        return 0
    else
        echo -e "${RED}✗${NC}"
        return 1
    fi
}

cd "${REPO_ROOT}"

# Code Metrics
echo -e "\n${GREEN}[1/3] Code Metrics${NC}"
echo "-------------------"

run_tool "CLOC" \
    "cloc src/ include/ --md --by-file --report-file='${CODE_METRICS}/analysis_cloc_fresh_detailed.md'"

run_tool "Lizard" \
    "lizard src/ include/ -l cpp -C 15 -w -o '${CODE_METRICS}/analysis_lizard_fresh_detailed.txt'"

run_tool "PMcCabe" \
    "pmccabe src/**/*.cpp src/**/**/*.cpp > '${CODE_METRICS}/analysis_pmccabe_fresh.txt' || true"

run_tool "SLOCCount" \
    "sloccount src/ > '${CODE_METRICS}/analysis_sloccount_fresh.txt'"

# Static Analysis & Security
echo -e "\n${GREEN}[2/3] Static Analysis & Security${NC}"
echo "-----------------------------------"

run_tool "Flawfinder" \
    "flawfinder --quiet --dataonly src/ include/ > '${STATIC_ANALYSIS}/analysis_flawfinder_fresh.txt'"

run_tool "Cppcheck (kernel)" \
    "cppcheck --enable=warning,style,performance,portability --quiet --template='{file}:{line}: {severity}: {message}' src/kernel/*.cpp > '${STATIC_ANALYSIS}/analysis_cppcheck_kernel.txt' 2>&1 || true"

run_tool "Clang-Tidy (sample)" \
    "clang-tidy src/kernel/kernel.cpp -- -std=c++23 -I include/ > '${STATIC_ANALYSIS}/analysis_clang_tidy_sample.txt' 2>&1 || true"

# Miscellaneous Tools
echo -e "\n${GREEN}[3/3] Documentation & Navigation${NC}"
echo "---------------------------------"

run_tool "Tree structure" \
    "tree -L 3 -I 'build|.xmake|bin-*|third_party' > '${TOOLS_MISC}/analysis_tree_fresh.txt'"

run_tool "File count" \
    "find src/ include/ -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | wc -l > '${TOOLS_MISC}/file_count.txt'"

# Generate summary report
echo -e "\n${GREEN}Generating Summary...${NC}"
cat > "${OUTPUT_DIR}/analysis_summary.txt" << EOF
XINIM Code Analysis Summary
Generated: $(date)

Code Metrics:
-------------
$(tail -5 "${CODE_METRICS}/analysis_sloccount_fresh.txt")

Complexity (High CCN):
----------------------
$(grep -c "warning:" "${CODE_METRICS}/analysis_lizard_fresh_detailed.txt" || echo "0") functions with CCN > 15

Security Issues:
----------------
$(wc -l < "${STATIC_ANALYSIS}/analysis_flawfinder_fresh.txt") findings from Flawfinder

Files Analyzed:
---------------
$(cat "${TOOLS_MISC}/file_count.txt") C++ source files

Report Location:
----------------
${OUTPUT_DIR}

Individual reports:
- Code metrics: ${CODE_METRICS}/
- Static analysis: ${STATIC_ANALYSIS}/
- Tools & misc: ${TOOLS_MISC}/
EOF

echo -e "\n${GREEN}Analysis Complete!${NC}"
echo "Summary: ${OUTPUT_DIR}/analysis_summary.txt"
echo ""
cat "${OUTPUT_DIR}/analysis_summary.txt"
