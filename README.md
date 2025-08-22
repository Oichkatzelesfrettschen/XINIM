# XINIM

**XINIM** is a modern reimplementation of classic Minix, targeting contemporary **arm64** and **x86-64** machines using **C++23**. The project combines the simplicity and educational value of the original Minix design with modern programming practices and hardware capabilities.

## Quick Start

**Prerequisites:** C++23 compiler (Clang 18+ recommended), CMake, Ninja

```bash
# Install dependencies (see docs/TOOL_INSTALL.md for full details)
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake ninja-build clang-18 lld-18 lldb-18 \
    libsodium-dev nlohmann-json3-dev

# Build
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18
ninja -C build
```

## Documentation

- **[Building and Testing](docs/BUILDING.md)** — Complete build instructions and test procedures
- **[Tool Installation](docs/TOOL_INSTALL.md)** — Development dependencies and analysis tools
- **[Architecture](docs/ARCHITECTURE.md)** — System design and component overview

## Development Setup

For a complete development environment setup, follow the step-by-step instructions in [`tools/setup.md`](tools/setup.md).

## Project Structure

```
├── commands/          # User-space commands and utilities
├── crypto/            # Cryptographic subsystem (post-quantum ready)
├── docs/              # Documentation and architecture guides
├── fs/                # File system implementation
├── include/           # Public headers and API definitions
├── kernel/            # Kernel core and drivers
├── lib/               # Runtime library components
├── mm/                # Memory management subsystem
├── tests/             # Unit tests and test harnesses
└── tools/             # Build tools and utilities
```

## Key Features

- **Modern C++23** implementation maintaining Minix design principles
- **Cross-platform** support for arm64 and x86-64 architectures
- **Post-quantum cryptography** integration via Kyber/Dilithium
- **Comprehensive tooling** for analysis, debugging, and documentation
- **Educational focus** with extensive documentation and clear code structure

## Build Variants

The project supports multiple build configurations:

| Configuration | Purpose | Command |
|---------------|---------|---------|
| **Native** | Development and testing | Standard CMake build |
| **Cross-compile** | Target bare-metal x86-64 | `cmake -DCROSS_COMPILE_X86_64=ON` |
| **AT Driver** | Use AT-style disk controller | `cmake -DDRIVER_AT=ON` |
| **PC Driver** | Use PC/XT disk controller | `cmake -DDRIVER_PC=ON` |

## Testing

```bash
# Build and run unit tests
cmake -S tests -B build_test
ninja -C build_test
ctest --test-dir build_test

# Quick smoke test
make -C test f=t10a LIB=
```

## Contributing

1. Ensure you have the required C++23 toolchain (see [docs/TOOL_INSTALL.md](docs/TOOL_INSTALL.md))
2. Follow the build instructions in [docs/BUILDING.md](docs/BUILDING.md)
3. Install the git hooks: `ln -s ../../hooks/pre-commit .git/hooks/pre-commit`
4. All C++ files must be documented with Doxygen-style comments
5. Run `clang-format -i` on modified C++ files before committing

## License

See [LICENSE](LICENSE) for details.

## Status

This project is actively under development. While functional, it is intended primarily for educational purposes and research into operating system design patterns.