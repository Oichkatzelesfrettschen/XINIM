# Xinim Requirements

Version: 1.1
Date: 2025-12-31
Status: Active

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
- CMake 3.28+
- Conan 2.0+
- Clang 18+ (primary compiler)
- LLVM 18+ (lld, lldb)
- GCC 13+ (alternate compiler)
- Git 2.20+
- Python 3.10+

Optional tools (recommended):
- Doxygen
- Sphinx + Breathe + RTD theme
- Graphviz
- QEMU system emulators
- clang-format, clang-tidy, cppcheck
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
