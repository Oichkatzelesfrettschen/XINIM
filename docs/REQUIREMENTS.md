# Xinim Requirements

Version: 1.2
Date: 2026-02-26
Status: Active (updated during Phase 10 validation)

## Overview
Xinim is a C++23 microkernel OS targeting x86_64. The project standardizes on
CMake + Conan for all builds and dependency management. This document defines
runtime, development, and documentation requirements.

Build system: CMake (authoritative) + Conan (dependencies)
Languages: C++23 only (migration in progress)

## Runtime Requirements
Minimum hardware:
- CPU: x86_64 with SSE2 (AVX2/AVX512 recommended)
- RAM: 512 MB (2 GB recommended)
- Disk: 100 MB for OS image and runtime files

Supported platforms:
- QEMU x86_64 (primary)
- VirtualBox / VMware (secondary)
- Bare metal x86_64 (experimental)

## Development Requirements
Core tools (required):
- CMake 4.2+ (4.2.1 verified)
- Conan 2.24+ (2.24.0 verified)
- Clang 21+ (21.1.6 verified, primary compiler)
- LLVM 21+ (lld, lldb)
- Git 2.20+
- Python 3.10+

Optional tools (recommended):
- Doxygen
- Sphinx + Breathe + RTD theme
- Graphviz
- QEMU system emulators
- clang-format, clang-tidy, cppcheck

## Module and Package Requirements

### Build and Core Tooling (all modules)
- CMake 3.28+ (configure, build, and CTest integration)
- Conan 2.0+ (dependency resolution and toolchain generation)
- Clang 18+ and LLVM tools (compiler, lld, lldb)
- Ninja (preferred generator)
- Python 3.10+ (scripts, tooling, docs)
- Git 2.20+ (source control)

### Kernel, HAL, Boot, and Drivers
- In-tree FIPS 202 (SHA3/SHAKE) and NTT implementations (no external crypto dep)
- libsodium (via Conan) for source extraction only; vendored functions for freestanding
- limine headers (vendored under third_party/limine)
- QEMU 10.1+ system emulators for boot validation
- No NASM required; all assembly is GAS syntax (.S files)

### Userland, Servers, and Tests
- CTest (via CMake) for 22 registered tests (20 host-side unit, 2 QEMU integration)
- xinim_add_host_test() CMake macro for C++23 host-compiled test binaries
- QEMU for boot smoke test and kshell validation
- Python 3 for kshell_test.py programmatic test

### Documentation (API and Guides)
- Doxygen
- Sphinx
- Breathe
- Sphinx RTD theme
- Graphviz

### Analysis and Quality Gates
- clang-format, clang-tidy, cppcheck
- lizard, cloc, sloccount, pmccabe
- flawfinder (security scan)
## Installation (Arch / CachyOS)
```bash
sudo pacman -Syu --needed \
  cmake \
  ninja \
  conan \
  clang \
  llvm \
  lld \
  lldb \
  libc++ \
  git \
  python \
  doxygen \
  graphviz \
  python-sphinx \
  python-sphinx-rtd-theme \
  python-breathe \
  qemu-system-x86
```

## Installation (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  git \
  clang-18 \
  lld-18 \
  lldb-18 \
  libc++-18-dev \
  libc++abi-18-dev \
  doxygen \
  graphviz \
  qemu-system-x86
pip install --user conan sphinx breathe sphinx-rtd-theme
```
## Installation (Fedora/RHEL)
```bash
sudo dnf install -y \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  git \
  clang \
  llvm \
  lld \
  lldb \
  libcxx-devel \
  doxygen \
  graphviz \
  qemu-system-x86
pip install --user conan sphinx breathe sphinx-rtd-theme
```

## Installation (macOS)
```bash
brew install cmake ninja conan llvm doxygen graphviz qemu
pip3 install --user sphinx breathe sphinx-rtd-theme
```

## Conan + CMake Build (Recommended)
```bash
# Detect profile
conan profile detect --force

# Install deps + generate toolchain in build/
conan install . -s build_type=Debug -of build --build=missing

# Or use the repo profile
conan install . -s build_type=Debug -of build --build=missing -pr conan/profiles/xinim-clang

# Configure + build
cmake --preset debug
cmake --build --preset debug
```
## Toolchain Build Dependencies
Xinim uses a custom x86_64-xinim-elf cross toolchain. See:
- docs/TOOLCHAIN_BUILD_DEPENDENCIES.md
- docs/TOOLCHAIN_SPECIFICATION.md

Typical dependencies (Linux):
- build-essential or base-devel
- bison, flex, gmp, mpfr, mpc, isl
- texinfo, help2man, gawk
- autoconf, automake, libtool
- pkg-config, wget, tar, xz, bzip2

## Verification
```bash
cmake --version
conan --version
clang --version
qemu-system-x86_64 --version
```

## Related Documentation
- docs/BUILDING.md
- docs/BUILD.md
- docs/TOOL_INSTALL.md
- docs/TOOLCHAIN_VERSIONS.md
- CONTRIBUTING.md
- docs/sphinx/conf.py
