# XINIM Requirements

**Version:** 1.0
**Date:** 2025-11-19
**Status:** ✅ COMPREHENSIVE

---

## Table of Contents

1. [Overview](#overview)
2. [Runtime Requirements](#runtime-requirements)
3. [Development Requirements](#development-requirements)
4. [Toolchain Requirements](#toolchain-requirements)
5. [Platform Support](#platform-support)
6. [Quick Start Installation](#quick-start-installation)
7. [Verification](#verification)
8. [Related Documentation](#related-documentation)

---

## Overview

XINIM is a modern C++23 microkernel operating system targeting x86_64 architecture. This document outlines all requirements for building, developing, and running XINIM.

**Build System:** xmake (primary), scripts for cross-compiler toolchain

**NOTE:** An outdated BUILD.md file mentions CMake, but XINIM **does not use CMake**. The correct build system is **xmake**.

---

## Runtime Requirements

### Minimum Hardware

| Component | Requirement | Notes |
|-----------|-------------|-------|
| CPU | x86_64 with SSE2 | AVX2/AVX512 recommended for crypto |
| RAM | 512 MB | 2+ GB recommended |
| Disk | 100 MB | For OS image and runtime files |
| Display | VGA compatible | Optional (can run headless) |

### Supported Platforms

- **QEMU x86_64** (primary development/testing)
- **VirtualBox** (tested)
- **VMware** (should work)
- **Bare metal x86_64** (experimental)

---

## Development Requirements

### Core Tools (Required)

| Tool | Min Version | Purpose |
|------|-------------|---------|
| **xmake** | 2.7.0+ | Primary build system |
| **Clang** | 18.0+ | C++23 compiler (primary) |
| **LLVM** | 18.0+ | lld linker, lldb debugger |
| **GCC** | 13.0+ | Alternative compiler (for toolchain build) |
| **Git** | 2.20+ | Version control |
| **Python** | 3.8+ | Build scripts |

### Additional Development Tools

| Tool | Purpose | Required? |
|------|---------|-----------|
| **Doxygen** | API documentation | Recommended |
| **pre-commit** | Git hooks | Recommended |
| **QEMU** | Testing/debugging | Recommended |
| **clang-format** | Code formatting | Optional |
| **clang-tidy** | Static analysis | Optional |
| **cppcheck** | Additional analysis | Optional |

### Installation (Ubuntu/Debian)

```bash
# Install xmake
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile

# Install Clang 18 toolchain
sudo apt-get install -y \
    clang-18 \
    lld-18 \
    lldb-18 \
    libc++-18-dev \
    libc++abi-18-dev

# Create symlinks for default clang
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100

# Install additional tools
sudo apt-get install -y \
    git \
    python3 \
    doxygen \
    qemu-system-x86

# Install pre-commit (optional)
pip install pre-commit
```

### Installation (Fedora/RHEL)

```bash
# Install xmake
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile

# Install Clang/LLVM
sudo dnf install -y \
    clang \
    llvm \
    lld \
    lldb \
    libcxx-devel

# Install additional tools
sudo dnf install -y \
    git \
    python3 \
    doxygen \
    qemu-system-x86
```

### Installation (Arch Linux)

```bash
# Install xmake
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile

# Install Clang/LLVM and tools
sudo pacman -Syu --needed \
    clang \
    llvm \
    lld \
    lldb \
    libc++ \
    git \
    python \
    doxygen \
    qemu-system-x86
```

### Installation (macOS)

```bash
# Install Homebrew if needed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install xmake
brew install xmake

# Install Clang/LLVM (Xcode Command Line Tools)
xcode-select --install

# Install additional tools
brew install \
    git \
    python3 \
    doxygen \
    qemu
```

---

## Toolchain Requirements

XINIM uses a custom x86_64-xinim-elf cross-compiler toolchain built from source.

### Toolchain Components

| Component | Version | Purpose |
|-----------|---------|---------|
| **binutils** | 2.41 | Assembler, linker, archiver |
| **GCC** | 13.2.0 | C/C++ cross-compiler |
| **dietlibc** | 0.34 | Minimal C library |

### Toolchain Build Dependencies

**Required packages (23 total):**

```bash
# Ubuntu/Debian
sudo apt-get install -y \
    build-essential \
    bison \
    flex \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    libisl-dev \
    texinfo \
    help2man \
    gawk \
    libc6-dev \
    linux-headers-$(uname -r) \
    git \
    wget \
    tar \
    xz-utils \
    bzip2 \
    m4 \
    autoconf \
    automake \
    libtool \
    pkg-config \
    python3
```

**Disk space:** ~10 GB free (5 GB for build, 800 MB for installed toolchain)
**Build time:** 1-4 hours (depends on CPU cores)

### Building the Toolchain

```bash
# Navigate to toolchain scripts
cd scripts/

# Check dependencies
./check_dependencies.sh

# Build toolchain (automated)
./setup_build_environment.sh
./build_binutils.sh
./build_gcc_stage1.sh
./build_dietlibc.sh
./build_gcc_stage2.sh

# Validate
source /opt/xinim-toolchain/xinim-env.sh
./validate_toolchain.sh
```

**See detailed documentation:**
- `docs/TOOLCHAIN_BUILD_DEPENDENCIES.md` - Complete dependency list
- `docs/WEEK1_SYSTEM_DEPENDENCIES_EXHAUSTIVE.md` - Exhaustive build guide (1000+ lines)
- `docs/TOOLCHAIN_SPECIFICATION.md` - Toolchain architecture

---

## Platform Support

### Tested Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| Ubuntu 22.04 LTS | ✅ Full | Recommended |
| Ubuntu 20.04 LTS | ✅ Full | |
| Debian 12 | ✅ Full | |
| Fedora 38+ | ✅ Full | |
| Arch Linux | ✅ Full | Rolling release |
| macOS 12+ | 🟡 Partial | Requires Homebrew, GNU tools |
| WSL2 (Ubuntu) | ✅ Full | Windows Subsystem for Linux |
| Alpine Linux | 🟡 Partial | musl libc compatibility issues |

### Hardware Requirements (Development)

| Component | Minimum | Recommended | Optimal |
|-----------|---------|-------------|---------|
| **CPU** | 2 cores, 2.0 GHz | 4 cores, 2.5 GHz | 8+ cores, 3.0+ GHz |
| **RAM** | 4 GB | 8 GB | 16+ GB |
| **Disk** | 10 GB HDD | 20 GB SSD | 50+ GB NVMe |
| **Network** | 10 Mbps | 50 Mbps | 100+ Mbps |

---

## Quick Start Installation

### Option 1: Full Development Environment

```bash
# 1. Install xmake
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile

# 2. Install Clang 18
sudo apt-get install -y clang-18 lld-18 lldb-18 libc++-18-dev

# 3. Clone XINIM
git clone https://github.com/Oichkatzelesfrettschen/XINIM.git
cd XINIM

# 4. Configure and build
xmake config --toolchain=clang --mode=debug
xmake build

# 5. Run tests
xmake run test_suite
```

### Option 2: Minimal Build (xmake only)

```bash
# Install xmake
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile

# Build with system compiler
cd XINIM
xmake
```

### Option 3: With Custom Toolchain

```bash
# 1. Build toolchain first
cd XINIM/scripts
./setup_build_environment.sh
# ... (follow toolchain build process)

# 2. Build XINIM with custom toolchain
source /opt/xinim-toolchain/xinim-env.sh
cd ..
xmake config --toolchain=xinim-elf
xmake build
```

---

## Verification

### Verify xmake Installation

```bash
xmake --version
# Expected: xmake v2.7.0+
```

### Verify Clang Installation

```bash
clang --version
# Expected: clang version 18.0.0 or newer

clang++ --version
# Should show C++23 support
```

### Verify XINIM Build

```bash
cd XINIM
xmake config
xmake build

# Should complete without errors
# Output: build/xinim (kernel image) or similar
```

### Verify Toolchain (if built)

```bash
source /opt/xinim-toolchain/xinim-env.sh
x86_64-xinim-elf-gcc --version
# Expected: x86_64-xinim-elf-gcc (GCC) 13.2.0

# Compile test
echo 'int main(){return 0;}' | x86_64-xinim-elf-gcc -x c - -o test
file test
# Expected: ELF 64-bit LSB executable, x86-64, statically linked
```

### Automated Verification Scripts

```bash
# Check development dependencies
./scripts/check_dev_dependencies.sh

# Check toolchain dependencies (if building toolchain)
./scripts/check_dependencies.sh

# Validate built toolchain
./scripts/validate_toolchain.sh
```

---

## Related Documentation

### Build System

- `README.md` - Project overview, quick start (**CORRECT: uses xmake**)
- `docs/BUILD.md` - **OUTDATED: mentions CMake (ignore this)**
- `xmake.lua` - **ACTUAL build configuration**

### Toolchain

- `docs/TOOLCHAIN_SPECIFICATION.md` - Toolchain architecture and design
- `docs/TOOLCHAIN_BUILD_DEPENDENCIES.md` - Complete dependency list (565 lines)
- `docs/WEEK1_SYSTEM_DEPENDENCIES_EXHAUSTIVE.md` - Exhaustive guide (1000+ lines)
- `docs/WEEK1_IMPLEMENTATION_GUIDE.md` - Step-by-step toolchain build
- `docs/DIETLIBC_INTEGRATION_GUIDE.md` - dietlibc specifics

### Development

- `docs/CONTRIBUTING.md` - Contribution guidelines
- `docs/DEVELOPER_GUIDE.md` - Development practices (TODO)
- `docs/api/` - Doxygen API documentation (generated)

### Dependencies Lifecycle

- `docs/DEPENDENCIES.md` - Dependency update and deprecation policy

---

## Environment Variables

### Development Environment

```bash
# xmake configuration
export XMAKE_ROOT=$HOME/.xmake

# C++ standard
export CXXFLAGS="-std=c++23"

# Compiler selection
export CC=clang-18
export CXX=clang++-18
```

### Toolchain Environment (if using custom toolchain)

```bash
# Source toolchain environment
source /opt/xinim-toolchain/xinim-env.sh

# Sets:
# - XINIM_PREFIX=/opt/xinim-toolchain
# - XINIM_SYSROOT=/opt/xinim-sysroot
# - XINIM_TARGET=x86_64-xinim-elf
# - CC, CXX, AR, LD, etc. to cross-compiler tools
# - PATH updated with toolchain/bin
```

---

## Troubleshooting

### "xmake: command not found"

**Solution:** Install xmake and source profile:
```bash
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile
```

### "error: unknown type name 'concept'"

**Solution:** Ensure C++23 support:
```bash
clang++ --version  # Should be 18.0+
xmake config --mode=debug --verbose
# Check that -std=c++23 is being used
```

### Build fails with CMake errors

**Solution:** XINIM doesn't use CMake. Ignore `BUILD.md`. Use xmake:
```bash
xmake config
xmake build
```

### Toolchain build fails

**Solution:** Check dependencies:
```bash
./scripts/check_dependencies.sh
# Install missing packages
sudo apt-get install [missing-packages]
```

**See detailed troubleshooting:** `docs/WEEK1_SYSTEM_DEPENDENCIES_EXHAUSTIVE.md` Section 9

---

## Summary

**Minimum to build XINIM:**
- xmake 2.7.0+
- Clang 18+ or GCC 13+
- Git, Python 3.8+
- 10 GB free disk space
- 4 GB RAM

**Minimum to build toolchain:**
- 23 system packages
- 10 GB free disk space
- 4 GB RAM (8 GB recommended)
- 1-4 hours build time

**Recommended setup:**
- Ubuntu 22.04 LTS
- Clang 18 toolchain
- xmake build system
- QEMU for testing
- 8+ GB RAM, 8+ CPU cores

---

## Appendix: Package Lists by Distribution

### Ubuntu/Debian (Development)

```bash
sudo apt-get install -y \
    xmake \
    clang-18 \
    lld-18 \
    lldb-18 \
    libc++-18-dev \
    libc++abi-18-dev \
    git \
    python3 \
    doxygen \
    qemu-system-x86 \
    pre-commit
```

**Note:** xmake may need manual installation via curl (see above)

### Fedora (Development)

```bash
sudo dnf install -y \
    clang \
    llvm \
    lld \
    lldb \
    libcxx-devel \
    git \
    python3 \
    doxygen \
    qemu-system-x86

# xmake manual install
curl -fsSL https://xmake.io/shget.text | bash
```

### Arch (Development)

```bash
sudo pacman -Syu --needed \
    xmake \
    clang \
    llvm \
    lld \
    lldb \
    libc++ \
    git \
    python \
    doxygen \
    qemu-system-x86
```

---

**Document Maintainer:** XINIM Development Team
**Last Updated:** 2025-11-19
**Supersedes:** BUILD.md (outdated)
**Related Issues:** BUILD.md incorrectly documents CMake as build system
