# Toolchain Installation

This document describes the development dependencies required for building the
project. Step-by-step installation commands live in [`tools/setup.md`](../tools/setup.md).
The sections below catalog the packages that have proven successful in the
current environment and highlight additional tools worth exploring.

## Package Inventory

| Package | Purpose |
| --- | --- |
| build-essential | GNU compiler suite and basic development utilities |
| cmake | Cross-platform build system generator |
| nasm | Assembler for low-level components |
| ninja-build | High-speed build tool used with CMake |
| clang-18 | C/C++ compiler with C++23 support |
| lld-18 | LLVM linker |
| lldb-18 | LLVM debugger |
| clang-tidy | Static analysis for C/C++ |
| clang-format | Source formatting |
| clang-tools | Additional Clang utilities |
| clangd | Language server protocol implementation |
| llvm | Core LLVM infrastructure |
| llvm-dev | LLVM development headers |
| libclang-dev | C interface to Clang |
| libclang-cpp-dev | C++ interface to Clang |
| gcc-x86-64-linux-gnu | Cross GCC compiler (C) |
| g++-x86-64-linux-gnu | Cross GCC compiler (C++) |
| binutils-x86-64-linux-gnu | Cross binutils suite |
| cppcheck | Static code analyzer |
| valgrind | Dynamic analysis and memory debugging |
| lcov | Code coverage reporting |
| strace | System call tracer |
| gdb | GNU debugger |
| doxygen | API documentation generator |
| graphviz | Diagram generation for documentation |
| python3-sphinx | Sphinx documentation engine |
| python3-breathe | Bridge between Doxygen and Sphinx |
| python3-sphinx-rtd-theme | Read the Docs theme |
| python3-pip | Python package manager |
| libsodium-dev | libsodium development headers |
| libsodium23 | libsodium runtime library |
| nlohmann-json3-dev | JSON serialization library |
| qemu-system-x86 | Virtual machine for x86 testing |
| qemu-utils | QEMU helper utilities |
| qemu-user | User-mode emulation |
| rustc | Rust compiler |
| cargo | Rust package manager |
| rustfmt | Rust code formatter |
| afl++ | Fuzz-testing framework |
| tmux | Terminal multiplexer |
| cloc | Lines-of-code counter |
| cscope | Source code navigation |
| ack | Fast text searching |
| linux-tools-common | Kernel performance tools |
| linux-tools-generic | Additional kernel tools |
| linux-tools-$(uname -r) | Matching kernel tools for the current kernel |

### Python Packages

- `lizard` — computes cyclomatic complexity metrics.

### NPM Packages

- `markdownlint-cli` — lints Markdown documentation for style issues.

## Additional Tools to Explore

- `universal-ctags` for generating cross-language symbol indexes.
- `bear` to capture compile commands for better IDE integration.
- `include-what-you-use` to verify minimal C/C++ header inclusion.
- AddressSanitizer, UndefinedBehaviorSanitizer, and other Clang sanitizers for
  runtime issue detection.
- `perf` and `gprof` for detailed performance profiling.
