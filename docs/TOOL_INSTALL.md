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
| libc++-18-dev | LLVM C++ standard library headers |
| libc++abi-18-dev | LLVM C++ ABI library |
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
| bear | Capture compile commands for IDEs |
| sloccount | Historical code size analysis |
| flawfinder | Detects potential security flaws |
| universal-ctags | Cross-language symbol index generator |
| linux-tools-common | Kernel performance tools |
| linux-tools-generic | Additional kernel tools |
| linux-tools-$(uname -r) | Matching kernel tools for the current kernel |

### Python Packages

- `lizard` — computes cyclomatic complexity metrics.
- `diffoscope` — compares files, archives, and directories recursively.
- `pylint` — static analysis for Python code quality.
- `flake8` — style and lint checks for Python.
- `mypy` — optional static typing for Python.
- `semgrep` — semantic code analysis engine.

### NPM Packages

- `markdownlint-cli` — lints Markdown documentation for style issues.
- `eslint` — linting for JavaScript and TypeScript.
- `jshint` — alternative JavaScript linter.
- `jscpd` — copy–paste detector for multiple languages.
- `nyc` — Istanbul command-line interface for coverage.

## Additional Tools to Explore

- `include-what-you-use` to verify minimal C/C++ header inclusion (may require
  manual installation).
- AddressSanitizer, UndefinedBehaviorSanitizer, and other Clang sanitizers for
  runtime issue detection.
- `perf` and `gprof` for detailed performance profiling.

## Installation Methods

The table below distills canonical installation pathways for common analysis tools, referencing official package managers or source repositories. Usage examples correspond to the commands executed in the files under the [`analysis/`](analysis/) directory.

| Tool      | Install Method | Install Command                                         | Usage Example                      |
|-----------|----------------|---------------------------------------------------------|------------------------------------|
| lizard    | pip            | `pip install lizard`                                   | `lizard fs kernel include`         |
| cloc      | apt            | `sudo apt install cloc`                                | `cloc .`                           |
| cscope    | apt            | `sudo apt install cscope`                              | `cscope -Rbq`                      |
| diffoscope| pip            | `pip install diffoscope`                               | `diffoscope README.md docs/README_renamed.md` |
| dtrace    | source         | `git clone https://github.com/dtrace4linux/linux.git; cd linux; make; sudo make install` | `dtrace -l` (manual build) |
| valgrind  | apt            | `sudo apt install valgrind`                            | `valgrind ls`                      |
| cppcheck  | apt            | `sudo apt install cppcheck`                            | `cppcheck --enable=warning fs`     |
| sloccount | apt            | `sudo apt install sloccount`                           | `sloccount .`                      |
| flawfinder| apt            | `sudo apt install flawfinder`                          | `flawfinder .`                     |
| gdb       | apt            | `sudo apt install gdb`                                 | `gdb --version`                    |
| pylint    | pip            | `pip install pylint`                                   | `pylint $(git ls-files '*.py')`    |
| flake8    | pip            | `pip install flake8`                                   | `flake8 $(git ls-files '*.py')`    |
| mypy      | pip            | `pip install mypy`                                     | `mypy $(git ls-files '*.py')`      |
| semgrep   | pip            | `pip install semgrep`                                  | `semgrep --config=auto .`          |
| eslint    | npm            | `npm install --global eslint`                          | `eslint .`                         |
| jshint    | npm            | `npm install --global jshint`                          | `jshint .`                         |
| jscpd     | npm            | `npm install --global jscpd`                           | `jscpd --min-lines 50 .`           |
| nyc       | npm            | `npm install --global nyc`                             | `nyc --version`                    |
