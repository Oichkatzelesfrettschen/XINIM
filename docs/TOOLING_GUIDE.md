# XINIM Tooling & Analysis Guide

**Last Updated:** 2026-01-03  
**Status:** ✅ COMPLETE - All tools installed and configured

---

## Table of Contents

1. [Overview](#overview)
2. [Installed Tools](#installed-tools)
3. [Quick Start](#quick-start)
4. [Tool Usage](#tool-usage)
5. [CI/CD Integration](#cicd-integration)
6. [Quality Gates](#quality-gates)
7. [Best Practices](#best-practices)

---

## Overview

XINIM uses a comprehensive suite of static analysis, security scanning, and code quality tools to maintain high standards. All tools are integrated into the build system and can be run individually or as part of automated workflows.

### Tool Categories

- **Code Metrics:** Lines of code, complexity analysis, development effort estimation
- **Static Analysis:** Bug detection, portability issues, modern C++ checks
- **Security Scanning:** Vulnerability detection, buffer overflows, injection flaws
- **Code Coverage:** Test coverage analysis and reporting
- **Documentation:** API documentation generation and validation

---

## Installed Tools

### Analysis Tools

| Tool | Version | Purpose | Status |
|------|---------|---------|--------|
| **cloc** | 1.98 | Lines of code counting | ✅ Installed |
| **lizard** | 1.19.0 | Cyclomatic complexity | ✅ Installed |
| **pmccabe** | 2.8 | McCabe complexity | ✅ Installed |
| **sloccount** | 2.26 | Development effort | ✅ Installed |
| **flawfinder** | 2.0.19 | Security vulnerabilities | ✅ Installed |
| **cppcheck** | 2.13.0 | C++ static analysis | ✅ Installed |
| **clang-tidy** | 18.1.3 | Modern C++ linting | ✅ Installed |
| **clang-format** | 18.1.3 | Code formatting | ✅ Installed |
| **lcov** | 2.0-1 | Coverage reporting | ✅ Installed |
| **doxygen** | 1.9.8 | API documentation | ✅ Installed |
| **graphviz** | 2.43.0 | Diagram generation | ✅ Installed |

### Build Tools

| Tool | Purpose | Status |
|------|---------|--------|
| **xmake** | Primary build system | 🔴 Not installed (repo default) |
| **cmake** | Alternative build system | ✅ Available |
| **make** | Traditional build | ✅ Available |
| **ninja** | Fast build backend | ✅ Available |

---

## Quick Start

### Run All Analysis

```bash
# Run comprehensive analysis (recommended)
./tools/run_analysis.sh

# Results will be in:
# - docs/analysis/reports/code_metrics/
# - docs/analysis/reports/static_analysis/
# - docs/analysis/reports/tools_misc/
```

### Individual Tools

```bash
# Code metrics
cloc src/ include/ --md

# Complexity analysis
lizard src/ -l cpp -C 15

# Security scan
flawfinder src/ include/

# Static analysis
cppcheck --enable=all src/kernel/

# Format code
clang-format -i src/**/*.cpp

# Lint code
clang-tidy src/kernel/kernel.cpp -- -std=c++23 -Iinclude/
```

### Build with Sanitizers

```bash
# AddressSanitizer (memory errors)
xmake f -m debug
xmake build xinim-asan
xmake run xinim-asan

# ThreadSanitizer (data races)
xmake build xinim-tsan
xmake run xinim-tsan

# UndefinedBehaviorSanitizer
xmake build xinim-ubsan
xmake run xinim-ubsan
```

### Generate Coverage

```bash
# Build with coverage
xmake build xinim-coverage

# Run tests
xmake run test-all

# Generate report
xmake run xinim-coverage
# View: coverage_html/index.html
```

---

## Tool Usage

### 1. CLOC - Lines of Code

**Purpose:** Count lines of code, comments, and blank lines by language and file.

**Basic Usage:**
```bash
cloc src/ include/
```

**Detailed Report:**
```bash
cloc src/ include/ --md --by-file --report-file=cloc_report.md
```

**Example Output:**
```
Language      files    blank  comment     code
-------------------------------------------------
C++             432    12345    15678    64617
Assembly          8       52       89      519
C                 3       23       45      113
-------------------------------------------------
Total           443    12420    15812    65249
```

### 2. Lizard - Complexity Analysis

**Purpose:** Calculate cyclomatic complexity (CCN) for functions.

**Basic Usage:**
```bash
lizard src/ -l cpp
```

**Find High Complexity Functions:**
```bash
lizard src/ -l cpp -C 15 -w
```

**Output to File:**
```bash
lizard src/ -l cpp -C 15 -w -o complexity_report.txt
```

**Interpretation:**
- **CCN < 10:** Simple function ✅
- **CCN 10-15:** Moderate complexity 🟡
- **CCN 15-30:** High complexity, consider refactoring 🟠
- **CCN > 30:** Very complex, refactoring required 🔴

### 3. Flawfinder - Security Scanner

**Purpose:** Detect potential security vulnerabilities in C/C++ code.

**Basic Usage:**
```bash
flawfinder src/ include/
```

**Detailed Analysis:**
```bash
flawfinder --quiet --dataonly --html src/ > security_report.html
```

**Filter by Risk Level:**
```bash
# Only high-risk issues
flawfinder --minlevel=4 src/
```

**Common Issues:**
- Buffer overflows (strcpy, sprintf)
- Format string vulnerabilities (printf with user input)
- Race conditions (access, stat)
- Injection flaws (system, popen)

### 4. Cppcheck - Static Analysis

**Purpose:** Detect bugs, undefined behavior, and code quality issues.

**Basic Usage:**
```bash
cppcheck src/
```

**Enable All Checks:**
```bash
cppcheck --enable=all --std=c++23 src/
```

**XML Output for Tools:**
```bash
cppcheck --enable=all --xml --xml-version=2 src/ 2> cppcheck.xml
```

**Suppress False Positives:**
```cpp
// cppcheck-suppress unusedVariable
int temp = 0;
```

### 5. Clang-Tidy - Modern C++ Linting

**Purpose:** Enforce modern C++ best practices and detect bugs.

**Basic Usage:**
```bash
clang-tidy src/kernel/kernel.cpp -- -std=c++23 -Iinclude/
```

**With Compilation Database:**
```bash
# Generate compile_commands.json
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy -p build src/kernel/kernel.cpp
```

**Configuration (`.clang-tidy`):**
```yaml
Checks: '-*,
  bugprone-*,
  cert-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  portability-*,
  readability-*'
WarningsAsErrors: '*'
FormatStyle: file
HeaderFilterRegex: '.*'
```

**Auto-fix Issues:**
```bash
clang-tidy -p build --fix src/kernel/kernel.cpp
```

### 6. Clang-Format - Code Formatting

**Purpose:** Automatically format C++ code to consistent style.

**Format Single File:**
```bash
clang-format -i src/kernel/kernel.cpp
```

**Format All Files:**
```bash
find src/ include/ -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

**Check Without Modifying:**
```bash
clang-format --dry-run --Werror src/kernel/kernel.cpp
```

**Configuration (`.clang-format`):**
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
TabWidth: 4
UseTab: Never
```

### 7. LCOV - Coverage Analysis

**Purpose:** Measure test coverage and identify untested code.

**Generate Coverage Data:**
```bash
# 1. Build with coverage
gcc -fprofile-arcs -ftest-coverage src/kernel/kernel.cpp

# 2. Run tests
./test_suite

# 3. Capture coverage
lcov --capture --directory . --output-file coverage.info

# 4. Filter out system/third-party code
lcov --remove coverage.info '/usr/*' '*/third_party/*' -o coverage_filtered.info

# 5. Generate HTML report
genhtml coverage_filtered.info --output-directory coverage_html
```

**View Results:**
```bash
# Open in browser
xdg-open coverage_html/index.html
```

### 8. Doxygen - API Documentation

**Purpose:** Generate HTML documentation from code comments.

**Generate Documentation:**
```bash
doxygen Doxyfile
```

**View Documentation:**
```bash
xdg-open docs/html/index.html
```

**Example Doxygen Comment:**
```cpp
/**
 * @brief Allocates a DMA buffer for device communication
 * 
 * @param size Size of buffer in bytes
 * @param constraints DMA address constraints
 * @return std::expected<DMABuffer, std::error_code> Buffer or error
 * 
 * @note Buffer is automatically freed when DMABuffer is destroyed
 * @warning Must be called with interrupts disabled
 * 
 * @see DMABuffer, DMAPool
 * @since 1.0.0
 */
std::expected<DMABuffer, std::error_code> 
allocate_dma_buffer(size_t size, const DMAConstraints& constraints);
```

---

## CI/CD Integration

### GitHub Actions Workflow

Create `.github/workflows/analysis.yml`:

```yaml
name: Code Analysis

on:
  push:
    branches: [master, develop]
  pull_request:
  schedule:
    - cron: '0 0 * * 0'  # Weekly

jobs:
  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install tools
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            clang-18 clang-tidy-18 clang-format-18 \
            cppcheck lcov doxygen graphviz
          pip3 install lizard flawfinder
      
      - name: Run analysis
        run: ./tools/run_analysis.sh
      
      - name: Upload reports
        uses: actions/upload-artifact@v3
        with:
          name: analysis-reports
          path: docs/analysis/reports/

  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [asan, tsan, ubsan]
    steps:
      - uses: actions/checkout@v3
      
      - name: Build with ${{ matrix.sanitizer }}
        run: |
          xmake f -m debug
          xmake build xinim-${{ matrix.sanitizer }}
      
      - name: Run tests
        run: xmake run xinim-${{ matrix.sanitizer }}

  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Build with coverage
        run: |
          xmake build xinim-coverage
          xmake run test-all
          xmake run xinim-coverage
      
      - name: Upload coverage
        uses: codecov/codecov-action@v3
        with:
          files: ./coverage.info
```

### Pre-commit Hooks

Install pre-commit hooks:

```bash
# Copy hooks to .git/hooks/
cp tools/pre-commit-clang-format.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

Or use Python pre-commit framework:

```bash
pip install pre-commit
pre-commit install
```

`.pre-commit-config.yaml` is already configured in the repository.

---

## Quality Gates

### Code Quality Thresholds

| Metric | Threshold | Action |
|--------|-----------|--------|
| **Cyclomatic Complexity** | CCN > 15 | Refactor function |
| **Function Length** | > 100 lines | Split function |
| **File Length** | > 1000 lines | Split file |
| **Security Issues (High)** | > 0 | Block PR |
| **Security Issues (Medium)** | > 5 | Require review |
| **Test Coverage** | < 70% | Require more tests |
| **Clang-Tidy Errors** | > 0 | Fix before merge |
| **Format Check** | Not formatted | Auto-fix |

### Enforcement in CI

```yaml
# In GitHub Actions
- name: Check quality gates
  run: |
    # Complexity check
    COMPLEX=$(lizard src/ -l cpp -C 15 -w | grep -c "warning:")
    if [ $COMPLEX -gt 0 ]; then
      echo "::error::Found $COMPLEX high-complexity functions"
      exit 1
    fi
    
    # Security check
    HIGH_RISK=$(flawfinder --minlevel=4 src/ | grep -c "Hits")
    if [ $HIGH_RISK -gt 0 ]; then
      echo "::error::Found $HIGH_RISK high-risk security issues"
      exit 1
    fi
    
    # Format check
    clang-format --dry-run --Werror src/**/*.cpp
```

---

## Best Practices

### 1. Code Before Committing

```bash
# Format code
clang-format -i $(git diff --name-only --cached | grep -E '\.(cpp|hpp|h)$')

# Run quick checks
clang-tidy $(git diff --name-only --cached | grep '\.cpp$') -- -std=c++23 -Iinclude/

# Check for security issues
flawfinder $(git diff --name-only --cached | grep -E '\.(cpp|hpp)$')
```

### 2. Before Creating PR

```bash
# Run full analysis
./tools/run_analysis.sh

# Review reports
cat docs/analysis/reports/analysis_summary.txt

# Fix any critical issues found
```

### 3. Weekly Maintenance

```bash
# Update metrics
./tools/run_analysis.sh

# Review complexity trends
lizard src/ -l cpp -C 15 -w

# Check for new security issues
flawfinder --minlevel=3 src/

# Update documentation
doxygen Doxyfile
```

### 4. Refactoring Workflow

When refactoring complex functions:

1. **Measure baseline:** `lizard src/commands/make.cpp`
2. **Extract functions:** Break down high-CCN functions
3. **Add tests:** Unit test extracted functions
4. **Re-measure:** Verify CCN reduction
5. **Update docs:** Add Doxygen comments

### 5. Security Review Process

1. **Scan:** `flawfinder --minlevel=3 src/`
2. **Triage:** Classify as true positive or false positive
3. **Fix:** Replace unsafe functions with safe alternatives
4. **Verify:** Re-scan to confirm fix
5. **Document:** Add comments explaining security considerations

---

## Troubleshooting

### Cppcheck Segfaults

**Issue:** Cppcheck crashes with "Segmentation fault"

**Solution:** Run on smaller subsets
```bash
# Instead of entire src/
cppcheck src/kernel/*.cpp src/mm/*.cpp
```

### Clang-Tidy Missing Headers

**Issue:** "fatal error: 'header.hpp' file not found"

**Solution:** Provide include paths
```bash
clang-tidy src/kernel/kernel.cpp -- -std=c++23 -Iinclude/ -Iinclude/xinim/
```

Or use compilation database:
```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/kernel/kernel.cpp
```

### Coverage Not Generated

**Issue:** No .gcda files created

**Solution:** Ensure program runs to completion
```bash
# Compile with coverage
g++ -fprofile-arcs -ftest-coverage main.cpp

# Run program (must exit normally)
./a.out

# Check for .gcda files
ls *.gcda
```

---

## References

- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [Clang-Format Style Options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [LCOV User Guide](http://ltp.sourceforge.net/coverage/lcov.php)
- [Doxygen Manual](https://www.doxygen.nl/manual/)
- [Flawfinder Documentation](https://dwheeler.com/flawfinder/)
- [Cppcheck Manual](http://cppcheck.sourceforge.net/manual.pdf)
- [MISRA C++ Guidelines](https://www.misra.org.uk/)
- [CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/)

---

**Maintained by:** XINIM Development Team  
**Last Review:** 2026-01-03  
**Next Review:** 2026-04-03 (Quarterly)
