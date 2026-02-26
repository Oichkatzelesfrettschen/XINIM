# Build Instructions

This document describes the Xinim build flow using CMake + Conan on CachyOS/Arch.

**Toolchain**: Clang 21.1.6 (primary), CMake 4.2.1, Conan 2.24.0, Ninja, QEMU 10.1.2.

## Prerequisites

Install required packages on CachyOS/Arch Linux:

```sh
sudo pacman -Syu --needed \
    cmake ninja conan clang llvm lld lldb libc++ \
    git python doxygen graphviz \
    python-sphinx python-sphinx-rtd-theme python-breathe \
    qemu-system-x86
```

Key packages:

| Package | Purpose |
|---------|---------|
| cmake 3.28+ | Build system generator |
| conan 2.0+ | C/C++ package manager |
| clang 18+ | C++23 compiler (Clang 21 on CachyOS) |
| ninja | Fast parallel build |
| lldb | Debugger |
| libc++ | LLVM C++ standard library |
| doxygen | API documentation |
| qemu-system-x86 | x86-64 virtual machine for testing |
| cppcheck | Static analyzer |

## Configure and Build

```bash
# 1. Install Conan dependencies and generate toolchain
scripts/conan_install.sh . Debug conan/profiles/xinim-clang

# 2. Configure via CMake preset
cmake --preset debug

# 3. Build
cmake --build --preset debug
```

For a Release build:
```bash
scripts/conan_install.sh . Release conan/profiles/xinim-clang
cmake --preset release
cmake --build --preset release
```

## Running Tests

```bash
# Run all host-side unit tests
ctest --output-on-failure --test-dir build/Debug -L unit

# Run all tests (includes QEMU integration)
ctest --output-on-failure --test-dir build/Debug
```

## Documentation

```bash
# Generate Doxygen API docs
cmake --build --preset debug --target xinim_docs

# View generated docs
xdg-open build/Debug/docs/html/index.html
```

## Conan Profile

The Conan profile at `conan/profiles/xinim-clang` targets:
- Compiler: clang 21
- Standard library: libc++
- C++ standard: 23

## See Also

- `docs/REQUIREMENTS.md` - Full dependency version matrix
- `docs/testing/TEST_STRATEGY.md` - Testing approach and QEMU integration
- `CMakePresets.json` - CMake preset definitions (debug/release)
- `CMakeLists.txt` - Primary build configuration
