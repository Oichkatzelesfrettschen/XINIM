#!/bin/bash
# XINIM Advanced Profiling & Analysis Suite
# Integrates Valgrind, Flamegraph, and comprehensive coverage

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${REPO_ROOT}/analysis_results"
COVERAGE_DIR="${OUTPUT_DIR}/coverage"
VALGRIND_DIR="${OUTPUT_DIR}/valgrind"
PROFILE_DIR="${OUTPUT_DIR}/profiling"

echo -e "${GREEN}XINIM Advanced Analysis Suite${NC}"
echo "======================================"
echo "Repository: ${REPO_ROOT}"
echo "Output: ${OUTPUT_DIR}"
echo ""

# Create output directories
mkdir -p "${COVERAGE_DIR}" "${VALGRIND_DIR}" "${PROFILE_DIR}"

cd "${REPO_ROOT}"

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

# =============================================================================
# Part 1: Memory Analysis with Valgrind
# =============================================================================

echo -e "\n${BLUE}[1/5] Memory Analysis (Valgrind)${NC}"
echo "-----------------------------------"

# Check if test binaries exist
if [ -f "build/posix-test" ]; then
    TEST_BINARY="build/posix-test"
elif [ -f "./posix-test" ]; then
    TEST_BINARY="./posix-test"
else
    echo -e "${YELLOW}No test binary found, skipping Valgrind${NC}"
    TEST_BINARY=""
fi

if [ -n "$TEST_BINARY" ]; then
    echo "Using test binary: $TEST_BINARY"
    
    # Memory leak detection
    echo "  Running memory leak detection..."
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --verbose \
             --log-file="${VALGRIND_DIR}/memcheck.txt" \
             "$TEST_BINARY" 2>&1 | head -20 || true
    
    # Cache profiling
    echo "  Running cache profiling..."
    valgrind --tool=cachegrind \
             --cachegrind-out-file="${VALGRIND_DIR}/cachegrind.out" \
             "$TEST_BINARY" > /dev/null 2>&1 || true
    
    if [ -f "${VALGRIND_DIR}/cachegrind.out" ]; then
        cg_annotate "${VALGRIND_DIR}/cachegrind.out" > "${VALGRIND_DIR}/cachegrind_analysis.txt" 2>&1 || true
    fi
    
    # Helgrind (thread error detection)
    echo "  Running thread error detection..."
    valgrind --tool=helgrind \
             --log-file="${VALGRIND_DIR}/helgrind.txt" \
             "$TEST_BINARY" > /dev/null 2>&1 || true
    
    echo -e "${GREEN}Valgrind analysis complete${NC}"
    echo "Reports: ${VALGRIND_DIR}/"
else
    echo -e "${YELLOW}Skipping Valgrind (no test binary)${NC}"
fi

# =============================================================================
# Part 2: Code Coverage (lcov/gcov)
# =============================================================================

echo -e "\n${BLUE}[2/5] Code Coverage (lcov/gcov)${NC}"
echo "---------------------------------"

# Check if coverage data exists
if ls *.gcda > /dev/null 2>&1 || ls **/*.gcda > /dev/null 2>&1; then
    echo "  Capturing coverage data..."
    lcov --capture \
         --directory . \
         --output-file "${COVERAGE_DIR}/coverage.info" \
         --rc lcov_branch_coverage=1 2>&1 | grep -v "WARNING" || true
    
    echo "  Filtering coverage data..."
    lcov --remove "${COVERAGE_DIR}/coverage.info" \
         '/usr/*' \
         '*/third_party/*' \
         '*/test/*' \
         --output-file "${COVERAGE_DIR}/coverage_filtered.info" \
         --rc lcov_branch_coverage=1 2>&1 | grep -v "WARNING" || true
    
    echo "  Generating HTML report..."
    genhtml "${COVERAGE_DIR}/coverage_filtered.info" \
            --output-directory "${COVERAGE_DIR}/html" \
            --title "XINIM Code Coverage" \
            --legend \
            --demangle-cpp \
            --rc lcov_branch_coverage=1 2>&1 | tail -5 || true
    
    echo "  Generating summary..."
    lcov --summary "${COVERAGE_DIR}/coverage_filtered.info" \
         --rc lcov_branch_coverage=1 > "${COVERAGE_DIR}/summary.txt" 2>&1 || true
    
    echo -e "${GREEN}Coverage analysis complete${NC}"
    echo "Report: ${COVERAGE_DIR}/html/index.html"
    
    # Extract coverage percentage
    if [ -f "${COVERAGE_DIR}/summary.txt" ]; then
        COVERAGE=$(grep "lines\.\.\.\.\.\." "${COVERAGE_DIR}/summary.txt" | awk '{print $2}' || echo "N/A")
        echo "Coverage: $COVERAGE"
    fi
else
    echo -e "${YELLOW}No coverage data found${NC}"
    echo "Tip: Build with --coverage flag and run tests"
fi

# =============================================================================
# Part 3: Performance Profiling (perf + flamegraph)
# =============================================================================

echo -e "\n${BLUE}[3/5] Performance Profiling${NC}"
echo "----------------------------"

# Check if perf is available
if command -v perf > /dev/null 2>&1 && [ -n "$TEST_BINARY" ]; then
    echo "  Recording performance data..."
    perf record -F 99 -g -o "${PROFILE_DIR}/perf.data" "$TEST_BINARY" > /dev/null 2>&1 || true
    
    if [ -f "${PROFILE_DIR}/perf.data" ]; then
        echo "  Generating perf report..."
        perf report -i "${PROFILE_DIR}/perf.data" --stdio > "${PROFILE_DIR}/perf_report.txt" 2>&1 || true
        
        echo "  Generating flamegraph (if stackcollapse available)..."
        if command -v stackcollapse-perf.pl > /dev/null 2>&1; then
            perf script -i "${PROFILE_DIR}/perf.data" | \
                stackcollapse-perf.pl | \
                flamegraph.pl > "${PROFILE_DIR}/flamegraph.svg" 2>&1 || true
            
            if [ -f "${PROFILE_DIR}/flamegraph.svg" ]; then
                echo -e "${GREEN}Flamegraph generated${NC}"
                echo "Flamegraph: ${PROFILE_DIR}/flamegraph.svg"
            fi
        else
            echo -e "${YELLOW}stackcollapse-perf.pl not found, skipping flamegraph${NC}"
            echo "Install: git clone https://github.com/brendangregg/FlameGraph"
        fi
        
        echo -e "${GREEN}Performance profiling complete${NC}"
        echo "Report: ${PROFILE_DIR}/perf_report.txt"
    fi
else
    echo -e "${YELLOW}perf not available or no test binary${NC}"
fi

# =============================================================================
# Part 4: Formal Verification
# =============================================================================

echo -e "\n${BLUE}[4/5] Formal Verification (Z3)${NC}"
echo "--------------------------------"

if [ -f "specs/verify_all.py" ]; then
    python3 specs/verify_all.py 2>&1 | tail -20
    
    if [ -f "specs/reports/verification_report.md" ]; then
        cp "specs/reports/verification_report.md" "${OUTPUT_DIR}/"
        echo -e "${GREEN}Formal verification complete${NC}"
        echo "Report: ${OUTPUT_DIR}/verification_report.md"
    fi
else
    echo -e "${YELLOW}Formal verification script not found${NC}"
fi

# =============================================================================
# Part 5: Generate Comprehensive Report
# =============================================================================

echo -e "\n${BLUE}[5/5] Generating Comprehensive Report${NC}"
echo "---------------------------------------"

cat > "${OUTPUT_DIR}/analysis_summary.md" << 'EOF'
# XINIM Comprehensive Analysis Report

**Generated:** $(date)
**Analysis Types:** Memory, Coverage, Performance, Formal Verification

## Summary

This report aggregates results from multiple analysis tools:

### 1. Memory Analysis (Valgrind)

**Location:** `valgrind/`

**Reports:**
- `memcheck.txt` - Memory leak detection
- `cachegrind_analysis.txt` - Cache performance
- `helgrind.txt` - Thread error detection

**Key Findings:**
EOF

if [ -f "${VALGRIND_DIR}/memcheck.txt" ]; then
    echo "- Memory leaks:" $(grep "definitely lost" "${VALGRIND_DIR}/memcheck.txt" | head -1 || echo "N/A") >> "${OUTPUT_DIR}/analysis_summary.md"
else
    echo "- No memory analysis data available" >> "${OUTPUT_DIR}/analysis_summary.md"
fi

cat >> "${OUTPUT_DIR}/analysis_summary.md" << 'EOF'

### 2. Code Coverage (lcov/gcov)

**Location:** `coverage/html/index.html`

**Metrics:**
EOF

if [ -f "${COVERAGE_DIR}/summary.txt" ]; then
    tail -10 "${COVERAGE_DIR}/summary.txt" >> "${OUTPUT_DIR}/analysis_summary.md"
else
    echo "- No coverage data available" >> "${OUTPUT_DIR}/analysis_summary.md"
fi

cat >> "${OUTPUT_DIR}/analysis_summary.md" << 'EOF'

### 3. Performance Profiling (perf)

**Location:** `profiling/`

**Reports:**
- `perf_report.txt` - Performance statistics
- `flamegraph.svg` - Visual call graph

**Hotspots:**
EOF

if [ -f "${PROFILE_DIR}/perf_report.txt" ]; then
    head -20 "${PROFILE_DIR}/perf_report.txt" >> "${OUTPUT_DIR}/analysis_summary.md"
else
    echo "- No profiling data available" >> "${OUTPUT_DIR}/analysis_summary.md"
fi

cat >> "${OUTPUT_DIR}/analysis_summary.md" << 'EOF'

### 4. Formal Verification (Z3)

**Location:** `verification_report.md`

EOF

if [ -f "${OUTPUT_DIR}/verification_report.md" ]; then
    tail -20 "${OUTPUT_DIR}/verification_report.md" >> "${OUTPUT_DIR}/analysis_summary.md"
else
    echo "- No verification data available" >> "${OUTPUT_DIR}/analysis_summary.md"
fi

cat >> "${OUTPUT_DIR}/analysis_summary.md" << 'EOF'

## Recommendations

Based on the analysis results:

1. **Memory Safety:**
   - Review Valgrind reports for leaks
   - Fix any memory errors found
   - Ensure RAII patterns throughout

2. **Code Coverage:**
   - Target 70%+ coverage
   - Add tests for uncovered paths
   - Focus on critical components first

3. **Performance:**
   - Optimize hotspots identified in flamegraph
   - Review cache performance
   - Profile with realistic workloads

4. **Formal Verification:**
   - Maintain 100% verification rate
   - Add specs for new critical components
   - Integrate verification into CI/CD

## Next Steps

1. Review all reports in `analysis_results/`
2. Address critical issues first
3. Integrate analysis into development workflow
4. Run analysis regularly (weekly minimum)

---

**Analysis Tools Used:**
- Valgrind 3.22.0 - Memory & thread analysis
- lcov 2.0-1 - Code coverage
- perf - Performance profiling
- Z3 4.15.4 - Formal verification

**For questions or issues, refer to the tooling guide.**
EOF

echo -e "${GREEN}Comprehensive report generated${NC}"
echo "Report: ${OUTPUT_DIR}/analysis_summary.md"

# =============================================================================
# Final Summary
# =============================================================================

echo ""
echo "======================================"
echo -e "${GREEN}Analysis Complete!${NC}"
echo "======================================"
echo ""
echo "All results saved to: ${OUTPUT_DIR}/"
echo ""
echo "Key Reports:"
echo "  - Summary: analysis_summary.md"
echo "  - Coverage: coverage/html/index.html"
echo "  - Memory: valgrind/memcheck.txt"
echo "  - Performance: profiling/perf_report.txt"
echo "  - Verification: verification_report.md"
echo ""
echo "View summary:"
echo "  cat ${OUTPUT_DIR}/analysis_summary.md"
echo ""
