# XINIM Operating System

A modern C++23 operating system implementation focusing on POSIX compliance and advanced language features.

## Overview

XINIM is an experimental operating system written primarily in C++23, implementing POSIX utilities and system interfaces with modern C++ paradigms including concepts, ranges, coroutines, and modules.

## Features

- **Modern C++23**: Leverages latest language features for type safety and performance
- **POSIX Compliance**: Implements standard POSIX utilities and interfaces
- **Cross-platform Build**: Supports multiple architectures (x86_64, ARM64, RISC-V)
- **Modular Design**: Component-based architecture with clear separation of concerns
- **Advanced Optimization**: SIMD support with runtime CPU feature detection

## Project Structure

```
XINIM/
├── kernel/         # Kernel implementation
├── mm/            # Memory management
├── fs/            # Filesystem implementation
├── lib/           # Standard library
├── crypto/        # Cryptographic functions
├── commands/      # POSIX utilities
├── tests/         # Test suite
├── tools/         # Build and development tools
├── include/       # Header files
└── third_party/   # External dependencies
    ├── apache/    # Apache-licensed components
    └── gpl/       # GPL-licensed components (isolated)
```

## Building

### Prerequisites

- C++23 compatible compiler (Clang 17+ or GCC 13+)
- xmake build system
- Python 3.x (for tools)

### Quick Start

```bash
# Configure for release build
xmake f -m release

# Build the project
make build

# Or directly with xmake
xmake

# Run tests
make test

# Clean build artifacts
make clean
```

### Build Modes

- `debug`: Debug symbols, no optimization
- `release`: Full optimization, no debug symbols
- `profile`: Optimization with profiling support

## Development Tools

### Code Quality

```bash
# Run code quality audit
make audit

# Generate repository inventory
make inventory

# Format source code
make format

# Show repository statistics
make stats
```

### Available Tools

- `tools/repo_inventory.py`: Analyzes repository structure
- `tools/code_audit.sh`: Checks for code quality issues
- `tools/xinim_build_simple.cpp`: Native build system (BSD licensed)

## Testing

Tests are located in the `tests/` directory. Run all tests with:

```bash
xmake build -g test
xmake run -g test
```

## License

XINIM core components are licensed under the BSD 2-Clause License. See LICENSE file for details.

External components maintain their original licenses:
- Apache-licensed components in `third_party/apache/`
- GPL-licensed test suites in `third_party/gpl/` (not linked into binaries)

## Contributing

Contributions are welcome! Please ensure:
1. Code follows C++23 best practices
2. All tests pass
3. Code passes quality audit (`make audit`)
4. Documentation is updated as needed

## Status

This project is under active development. Core components are functional but not production-ready.

## Additional Documentation

For detailed architecture and implementation notes, see [docs/README_renamed.md](docs/README_renamed.md).

**XINIM** is a clean-room C++ 23 re-implementation of the original MINIX 1 educational operating system.  
The repository contains the full kernel, memory manager, file-system, classic user-land utilities, and supporting build scripts.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building](#building)
3. [Cleaning the Workspace](#cleaning-the-workspace)
4. [Documentation](#documentation)
5. [License](#license)

---

## Prerequisites

Install the tool-chain and host packages listed in **`docs/TOOL_INSTALL.md`**.
The step-by-step commands formerly automated by `setup.sh` now live in
[`tools/setup.md`](tools/setup.md). Example for Ubuntu 24.04 LTS:

```bash
sudo apt-get update
sudo apt-get install -y $(tr '\n' ' ' < docs/TOOL_INSTALL.md)
````

---

## Building

See [docs/BUILDING.md](docs/BUILDING.md) for canonical Ninja and Clang
instructions, including cross-compilation workflows and QEMU launch notes.

---

## Cleaning the Workspace

```bash
# Remove all build artefacts and test outputs
./tools/clean.sh
```

or, if using CMake directly:

```bash
cmake --build build --target clean
rm -rf build/
```

---

## Documentation

| Document                      | Description                         |
| ----------------------------- | ----------------------------------- |
| `docs/BUILDING.md`            | Full build and flashing guide       |
| `docs/ARCHITECTURE.md`        | Subsystem overview and design notes |
| `docs/TOOL_INSTALL.md`        | OS-specific dependency list         |
| `docs/sphinx/html/index.html` | Generated developer manual in HTML  |

---

## Sort Utility Merge Mode

The modern `sort` command supports a multi-file merge when invoked with the
`-m` flag. Each input file must already be sorted; the utility performs a
streaming k-way merge using the same comparison rules as regular sorting.  When
combined with the `-u` option, duplicate lines encountered across input files
are removed during the merge.

---

## License
Licensed under the **BSD-3-Clause** license. See `LICENSE` for details.
