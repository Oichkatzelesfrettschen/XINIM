#!/usr/bin/env bash
#
# validate_toolchain.sh - Validate XINIM cross-compiler toolchain installation
#
# This script validates that the x86_64-xinim-elf toolchain was built correctly
# by testing compilation of sample programs and checking binary properties.
#
# Usage: ./validate_toolchain.sh
#
# Exit codes:
#   0 - All validations passed
#   1 - One or more validations failed

set -euo pipefail

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Validation counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Paths
XINIM_PREFIX="${XINIM_PREFIX:-/opt/xinim-toolchain}"
XINIM_SYSROOT="${XINIM_SYSROOT:-/opt/xinim-sysroot}"
XINIM_TARGET="${XINIM_TARGET:-x86_64-xinim-elf}"

# Temp directory for test builds
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT

#
# Helper functions
#

info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

success() {
    echo -e "${GREEN}[PASS]${NC} $*"
    ((TESTS_PASSED++))
    ((TESTS_TOTAL++))
}

fail() {
    echo -e "${RED}[FAIL]${NC} $*"
    ((TESTS_FAILED++))
    ((TESTS_TOTAL++))
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

section() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE} $*${NC}"
    echo -e "${BLUE}========================================${NC}"
}

#
# Validation tests
#

validate_environment() {
    section "Validating Environment"

    # Check PREFIX exists
    if [[ -d "$XINIM_PREFIX" ]]; then
        success "Toolchain directory exists: $XINIM_PREFIX"
    else
        fail "Toolchain directory not found: $XINIM_PREFIX"
        return 1
    fi

    # Check SYSROOT exists
    if [[ -d "$XINIM_SYSROOT" ]]; then
        success "Sysroot directory exists: $XINIM_SYSROOT"
    else
        fail "Sysroot directory not found: $XINIM_SYSROOT"
        return 1
    fi

    # Check PATH
    if echo "$PATH" | grep -q "$XINIM_PREFIX/bin"; then
        success "Toolchain is in PATH"
    else
        warn "Toolchain not in PATH, adding temporarily"
        export PATH="$XINIM_PREFIX/bin:$PATH"
    fi
}

validate_binutils() {
    section "Validating binutils"

    local tools=(
        "as"      # Assembler
        "ld"      # Linker
        "ar"      # Archiver
        "nm"      # Symbol lister
        "objcopy" # Object file converter
        "objdump" # Object file inspector
        "ranlib"  # Archive index generator
        "readelf" # ELF file inspector
        "size"    # Section size lister
        "strings" # String extractor
        "strip"   # Symbol stripper
    )

    for tool in "${tools[@]}"; do
        local fullpath="$XINIM_PREFIX/bin/${XINIM_TARGET}-${tool}"
        if [[ -x "$fullpath" ]]; then
            success "Found: $tool"
        else
            fail "Missing: $tool"
        fi
    done

    # Test assembler
    cat > "$TEST_DIR/test.s" <<'EOF'
.section .text
.global _start
_start:
    mov $60, %rax    # exit syscall
    xor %rdi, %rdi   # status = 0
    syscall
EOF

    if "${XINIM_TARGET}-as" "$TEST_DIR/test.s" -o "$TEST_DIR/test.o" 2>/dev/null; then
        success "Assembler works"
    else
        fail "Assembler failed"
    fi

    # Test linker
    if "${XINIM_TARGET}-ld" "$TEST_DIR/test.o" -o "$TEST_DIR/test" 2>/dev/null; then
        success "Linker works"

        # Verify ELF format
        if file "$TEST_DIR/test" | grep -q "ELF 64-bit"; then
            success "Linker produces ELF64 binaries"
        else
            fail "Linker produces wrong binary format"
        fi
    else
        fail "Linker failed"
    fi
}

validate_gcc() {
    section "Validating GCC"

    # Check compiler exists
    if command -v "${XINIM_TARGET}-gcc" &>/dev/null; then
        success "GCC compiler found"

        # Check version
        local version
        version=$("${XINIM_TARGET}-gcc" --version | head -n1)
        info "GCC version: $version"

        if echo "$version" | grep -q "13\.2"; then
            success "GCC version is 13.2.0"
        else
            warn "GCC version is not 13.2.0"
        fi
    else
        fail "GCC compiler not found"
        return 1
    fi

    # Check g++ exists
    if command -v "${XINIM_TARGET}-g++" &>/dev/null; then
        success "G++ compiler found"
    else
        fail "G++ compiler not found"
    fi

    # Check supported languages
    local languages
    languages=$("${XINIM_TARGET}-gcc" -v 2>&1 | grep "Configured with")

    if echo "$languages" | grep -q "enable-languages=c,c++"; then
        success "C and C++ languages enabled"
    else
        warn "Language configuration unexpected"
    fi
}

validate_dietlibc() {
    section "Validating dietlibc"

    # Check headers exist
    local headers=(
        "stdio.h"
        "stdlib.h"
        "unistd.h"
        "string.h"
        "stdint.h"
        "sys/types.h"
    )

    for header in "${headers[@]}"; do
        if [[ -f "$XINIM_SYSROOT/usr/include/$header" ]]; then
            success "Found header: $header"
        else
            fail "Missing header: $header"
        fi
    done

    # Check libc.a exists
    if [[ -f "$XINIM_SYSROOT/lib/libc.a" ]]; then
        success "Found: libc.a"

        # Check size (dietlibc should be ~200-300 KB)
        local size
        size=$(stat -f%z "$XINIM_SYSROOT/lib/libc.a" 2>/dev/null || stat -c%s "$XINIM_SYSROOT/lib/libc.a" 2>/dev/null || echo "0")
        local size_kb=$((size / 1024))

        info "libc.a size: ${size_kb} KB"

        if ((size_kb >= 150 && size_kb <= 500)); then
            success "libc.a size is reasonable (150-500 KB)"
        else
            warn "libc.a size is unexpected (expected 150-500 KB, got ${size_kb} KB)"
        fi
    else
        fail "Missing: libc.a"
    fi
}

validate_c_compilation() {
    section "Validating C Compilation"

    # Create hello world
    cat > "$TEST_DIR/hello.c" <<'EOF'
#include <stdio.h>

int main(void) {
    printf("Hello from XINIM!\n");
    return 0;
}
EOF

    # Compile
    if "${XINIM_TARGET}-gcc" --sysroot="$XINIM_SYSROOT" \
        -Os -static \
        "$TEST_DIR/hello.c" -o "$TEST_DIR/hello" 2>/dev/null; then
        success "C compilation successful"

        # Check binary properties
        if file "$TEST_DIR/hello" | grep -q "ELF 64-bit LSB executable, x86-64"; then
            success "Binary is x86-64 ELF64"
        else
            fail "Binary format is incorrect"
        fi

        # Check size (should be ~8-20 KB with dietlibc)
        local size
        size=$(stat -f%z "$TEST_DIR/hello" 2>/dev/null || stat -c%s "$TEST_DIR/hello" 2>/dev/null || echo "0")
        local size_kb=$((size / 1024))

        info "Hello world size: ${size_kb} KB"

        if ((size_kb >= 5 && size_kb <= 30)); then
            success "Binary size is reasonable (5-30 KB)"
        else
            warn "Binary size is unexpected (expected 5-30 KB, got ${size_kb} KB)"
        fi

        # Check symbols
        if "${XINIM_TARGET}-nm" "$TEST_DIR/hello" | grep -q "main"; then
            success "Binary contains 'main' symbol"
        else
            fail "Binary missing 'main' symbol"
        fi
    else
        fail "C compilation failed"
    fi
}

validate_cpp_compilation() {
    section "Validating C++ Compilation"

    # Create C++ hello world
    cat > "$TEST_DIR/hello.cpp" <<'EOF'
#include <iostream>

int main() {
    std::cout << "Hello from XINIM with C++!" << std::endl;
    return 0;
}
EOF

    # Compile
    if "${XINIM_TARGET}-g++" --sysroot="$XINIM_SYSROOT" \
        -Os -static \
        "$TEST_DIR/hello.cpp" -o "$TEST_DIR/hello_cpp" 2>/dev/null; then
        success "C++ compilation successful"

        # Check binary properties
        if file "$TEST_DIR/hello_cpp" | grep -q "ELF 64-bit LSB executable, x86-64"; then
            success "C++ binary is x86-64 ELF64"
        else
            fail "C++ binary format is incorrect"
        fi

        # Check size (will be larger due to libstdc++, ~100-200 KB)
        local size
        size=$(stat -f%z "$TEST_DIR/hello_cpp" 2>/dev/null || stat -c%s "$TEST_DIR/hello_cpp" 2>/dev/null || echo "0")
        local size_kb=$((size / 1024))

        info "C++ hello world size: ${size_kb} KB"

        if ((size_kb >= 50 && size_kb <= 300)); then
            success "C++ binary size is reasonable (50-300 KB)"
        else
            warn "C++ binary size is unexpected (expected 50-300 KB, got ${size_kb} KB)"
        fi
    else
        fail "C++ compilation failed"
    fi
}

validate_syscall_integration() {
    section "Validating Syscall Integration"

    # Create program that uses syscalls
    cat > "$TEST_DIR/syscall_test.c" <<'EOF'
#include <unistd.h>
#include <sys/types.h>

int main(void) {
    // Test basic syscalls
    write(1, "test\n", 5);
    getpid();
    return 0;
}
EOF

    if "${XINIM_TARGET}-gcc" --sysroot="$XINIM_SYSROOT" \
        -Os -static \
        "$TEST_DIR/syscall_test.c" -o "$TEST_DIR/syscall_test" 2>/dev/null; then
        success "Syscall integration compiles"

        # Check for syscall instruction in binary
        if "${XINIM_TARGET}-objdump" -d "$TEST_DIR/syscall_test" | grep -q "syscall"; then
            success "Binary contains 'syscall' instruction"
        else
            warn "Binary doesn't use 'syscall' instruction (might use int 0x80)"
        fi
    else
        fail "Syscall integration failed"
    fi
}

validate_libgcc() {
    section "Validating libgcc"

    # Create program that requires libgcc
    cat > "$TEST_DIR/libgcc_test.c" <<'EOF'
#include <stdint.h>

// Force use of 128-bit division (requires libgcc)
uint64_t test_div128(uint64_t a, uint64_t b) {
    __uint128_t x = (__uint128_t)a << 64;
    return (uint64_t)(x / b);
}

int main(void) {
    return (int)test_div128(12345, 67);
}
EOF

    if "${XINIM_TARGET}-gcc" --sysroot="$XINIM_SYSROOT" \
        -Os -static \
        "$TEST_DIR/libgcc_test.c" -o "$TEST_DIR/libgcc_test" 2>/dev/null; then
        success "libgcc integration works"
    else
        fail "libgcc integration failed"
    fi
}

validate_cross_compilation() {
    section "Validating Cross-Compilation"

    # Verify we're actually cross-compiling
    if "${XINIM_TARGET}-gcc" -dumpmachine | grep -q "x86_64-xinim-elf"; then
        success "Target triplet is correct: x86_64-xinim-elf"
    else
        fail "Target triplet is incorrect"
    fi

    # Check that binaries are NOT executable on host (unless host is x86_64 Linux)
    if [[ -f "$TEST_DIR/hello" ]]; then
        if file "$TEST_DIR/hello" | grep -q "statically linked"; then
            success "Binary is statically linked"
        else
            warn "Binary is not statically linked"
        fi
    fi
}

print_summary() {
    section "Validation Summary"

    echo ""
    echo "Total tests: $TESTS_TOTAL"
    echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    echo ""

    if ((TESTS_FAILED == 0)); then
        echo -e "${GREEN}✅ All validations passed!${NC}"
        echo ""
        echo "The x86_64-xinim-elf toolchain is ready to use."
        echo ""
        echo "Environment variables:"
        echo "  export XINIM_PREFIX=$XINIM_PREFIX"
        echo "  export XINIM_SYSROOT=$XINIM_SYSROOT"
        echo "  export XINIM_TARGET=$XINIM_TARGET"
        echo "  export PATH=$XINIM_PREFIX/bin:\$PATH"
        echo ""
        echo "Compile a program:"
        echo "  ${XINIM_TARGET}-gcc --sysroot=\$XINIM_SYSROOT -Os -static hello.c -o hello"
        echo ""
        return 0
    else
        echo -e "${RED}❌ Some validations failed!${NC}"
        echo ""
        echo "Please review the failures above and check:"
        echo "  1. All build scripts completed successfully"
        echo "  2. No errors during toolchain build"
        echo "  3. Environment variables are set correctly"
        echo ""
        return 1
    fi
}

#
# Main
#

main() {
    info "XINIM Toolchain Validation"
    info "=========================="
    info "Toolchain: $XINIM_PREFIX"
    info "Sysroot:   $XINIM_SYSROOT"
    info "Target:    $XINIM_TARGET"
    info "Test dir:  $TEST_DIR"
    echo ""

    # Run validations
    validate_environment || true
    validate_binutils || true
    validate_gcc || true
    validate_dietlibc || true
    validate_c_compilation || true
    validate_cpp_compilation || true
    validate_syscall_integration || true
    validate_libgcc || true
    validate_cross_compilation || true

    # Print summary and return exit code
    print_summary
}

main "$@"
