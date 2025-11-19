# Toolchain Build Dependencies

**Document:** Prerequisites for building x86_64-xinim-elf toolchain
**Version:** 1.0
**Date:** 2025-11-17

---

## Overview

This document lists all system dependencies required to build the XINIM cross-compiler toolchain from source. The toolchain consists of:

- GNU binutils 2.41
- GCC 13.2.0 (Stage 1 and Stage 2)
- dietlibc 0.34
- libgcc and libstdc++

---

## System Requirements

### Hardware

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 2 cores | 8+ cores |
| RAM | 4 GB | 8+ GB |
| Disk space | 5 GB free | 10+ GB free |
| Architecture | x86_64 | x86_64 |

**Build time estimates:**
- 2-core system: ~4 hours
- 4-core system: ~2 hours
- 8-core system: ~1 hour 50 minutes

### Operating System

**Tested on:**
- Ubuntu 22.04 LTS ✅
- Debian 12 ✅
- Fedora 38+ ✅
- Arch Linux ✅

**Should work on:**
- Any recent Linux distribution with GCC 9+
- WSL2 (Windows Subsystem for Linux)
- macOS (with some adjustments)

**Known issues:**
- Alpine Linux: Requires additional packages due to musl libc
- macOS: GNU sed and coreutils needed (install via Homebrew)

---

## Package Dependencies

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    bison \
    flex \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    texinfo \
    libisl-dev \
    help2man \
    gawk \
    libc6-dev \
    git \
    wget \
    tar \
    xz-utils \
    m4 \
    autoconf \
    automake \
    libtool \
    pkg-config \
    python3
```

**Package count:** 23 packages (~300 MB download, ~1 GB installed)

### Fedora/RHEL/CentOS

```bash
sudo dnf install -y \
    gcc \
    gcc-c++ \
    make \
    bison \
    flex \
    gmp-devel \
    mpfr-devel \
    libmpc-devel \
    texinfo \
    isl-devel \
    help2man \
    gawk \
    glibc-devel \
    git \
    wget \
    tar \
    xz \
    m4 \
    autoconf \
    automake \
    libtool \
    pkgconfig \
    python3
```

### Arch Linux

```bash
sudo pacman -Syu --needed \
    base-devel \
    bison \
    flex \
    gmp \
    mpfr \
    libmpc \
    texinfo \
    isl \
    git \
    wget \
    xz \
    m4 \
    autoconf \
    automake \
    libtool \
    pkg-config \
    python
```

### Alpine Linux

```bash
sudo apk add \
    build-base \
    gcc \
    g++ \
    make \
    bison \
    flex \
    gmp-dev \
    mpfr-dev \
    mpc1-dev \
    texinfo \
    isl-dev \
    git \
    wget \
    tar \
    xz \
    m4 \
    autoconf \
    automake \
    libtool \
    pkgconfig \
    python3 \
    linux-headers
```

**Note:** Alpine uses musl libc, which may cause some compatibility issues. Recommended to use Ubuntu/Debian.

### macOS (Homebrew)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install \
    gcc \
    make \
    bison \
    flex \
    gmp \
    mpfr \
    libmpc \
    texinfo \
    isl \
    gnu-sed \
    gawk \
    coreutils \
    git \
    wget \
    xz \
    m4 \
    autoconf \
    automake \
    libtool \
    pkg-config \
    python3

# Add GNU tools to PATH
export PATH="/usr/local/opt/gnu-sed/libexec/gnubin:$PATH"
export PATH="/usr/local/opt/coreutils/libexec/gnubin:$PATH"
export PATH="/usr/local/opt/make/libexec/gnubin:$PATH"
```

**Note:** macOS builds may require additional configuration for case-sensitive filesystems and different library paths.

---

## Detailed Package Descriptions

### Build Tools

| Package | Purpose | Why Required |
|---------|---------|--------------|
| gcc | C compiler | Compiles binutils and GCC itself (host compiler) |
| g++ | C++ compiler | Required for building GCC and libstdc++ |
| make | Build automation | GNU Make 4.0+ required for all builds |
| bison | Parser generator | Required by GCC for parsing C/C++ |
| flex | Lexer generator | Required by binutils and GCC |
| m4 | Macro processor | Used by autoconf and GCC |
| autoconf | Configure generator | Generates configure scripts |
| automake | Makefile generator | Generates Makefiles |
| libtool | Library building | Shared library management |
| pkg-config | Library metadata | Finds library compile flags |

### GCC Dependencies

| Package | Purpose | Version Required |
|---------|---------|------------------|
| libgmp-dev | Arbitrary precision arithmetic | 4.2+ |
| libmpfr-dev | Floating-point arithmetic | 3.1.0+ |
| libmpc-dev | Complex number arithmetic | 0.8.1+ |
| libisl-dev | Integer set library | 0.15+ (optional but recommended) |

**Why these libraries?**
- GMP: Used for constant folding and optimization
- MPFR: Used for floating-point constant folding
- MPC: Used for complex number support in C99/C11
- ISL: Used for polyhedral optimization (Graphite)

### Documentation Tools

| Package | Purpose | Why Required |
|---------|---------|--------------|
| texinfo | Documentation generator | Generates GCC and binutils info pages |
| help2man | Man page generator | Generates man pages from --help output |

**Note:** Can be disabled with `--disable-doc` if not needed, but documentation is useful for debugging.

### Utilities

| Package | Purpose | Why Required |
|---------|---------|--------------|
| gawk | GNU AWK | Used by build scripts (more features than mawk) |
| git | Version control | Required to clone dietlibc |
| wget | File downloader | Downloads binutils and GCC tarballs |
| tar | Archive extractor | Extracts source tarballs |
| xz-utils | XZ decompressor | Decompresses .tar.xz files |
| python3 | Scripting | Used by some GCC build scripts |

### C Library Development

| Package | Purpose | Why Required |
|---------|---------|--------------|
| libc6-dev | Host C library headers | Required even though we're building our own libc |
| linux-headers | Kernel headers | Required for syscall numbers and types |

**Note:** Even though we're building dietlibc, the host system needs C library development files for the build tools themselves.

---

## Permission Requirements

### Root/Sudo Access

**Required for:**
- Installing /opt/xinim-toolchain
- Installing /opt/xinim-sysroot
- Installing system packages

**Alternatives:**
- Build to $HOME instead of /opt (modify scripts)
- Use Docker/container (see below)
- Use --prefix=$HOME/xinim-toolchain

### Disk Space

**Breakdown:**
```
Source downloads:     ~500 MB
Build directories:    ~4 GB
Installed toolchain:  ~800 MB
Total:                ~5.3 GB
```

**Cleanup:**
After successful build, you can remove build directories to reclaim ~4 GB.

---

## Verification Script

After installing dependencies, verify they're available:

```bash
#!/bin/bash
# check_dependencies.sh

check_command() {
    if command -v "$1" &>/dev/null; then
        echo "✅ $1 found: $($1 --version 2>/dev/null | head -n1)"
    else
        echo "❌ $1 NOT FOUND"
        return 1
    fi
}

echo "Checking build dependencies..."
echo ""

check_command gcc
check_command g++
check_command make
check_command bison
check_command flex
check_command m4
check_command autoconf
check_command automake
check_command libtool
check_command git
check_command wget
check_command tar
check_command xz
check_command gawk
check_command python3

echo ""
echo "Checking development libraries..."

# Check for header files
check_header() {
    if [[ -f "/usr/include/$1" ]] || [[ -f "/usr/local/include/$1" ]]; then
        echo "✅ $1 found"
    else
        echo "❌ $1 NOT FOUND"
        return 1
    fi
}

check_header "gmp.h"
check_header "mpfr.h"
check_header "mpc.h"

echo ""
echo "Dependency check complete!"
```

Save as `check_dependencies.sh`, run with:
```bash
chmod +x check_dependencies.sh
./check_dependencies.sh
```

---

## Docker Alternative

If you don't have root access or want a clean build environment:

### Dockerfile

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        bison \
        flex \
        libgmp-dev \
        libmpfr-dev \
        libmpc-dev \
        texinfo \
        libisl-dev \
        help2man \
        gawk \
        libc6-dev \
        git \
        wget \
        tar \
        xz-utils \
        m4 \
        autoconf \
        automake \
        libtool \
        pkg-config \
        python3 && \
    rm -rf /var/lib/apt/lists/*

# Create build user (optional, for non-root builds)
RUN useradd -m -s /bin/bash builder && \
    mkdir -p /opt/xinim-toolchain /opt/xinim-sysroot && \
    chown -R builder:builder /opt/xinim-toolchain /opt/xinim-sysroot

USER builder
WORKDIR /home/builder

CMD ["/bin/bash"]
```

### Build and Run

```bash
# Build Docker image
docker build -t xinim-toolchain .

# Run container with XINIM sources mounted
docker run -it -v $(pwd):/xinim xinim-toolchain

# Inside container, build toolchain
cd /xinim/scripts
./setup_build_environment.sh
./build_binutils.sh
./build_gcc_stage1.sh
./build_dietlibc.sh
./build_gcc_stage2.sh
```

---

## Common Issues

### Issue: "configure: error: Building GCC requires GMP 4.2+, MPFR 3.1.0+ and MPC 0.8.1+"

**Solution:** Install development packages:
```bash
# Ubuntu/Debian
sudo apt-get install libgmp-dev libmpfr-dev libmpc-dev

# Fedora
sudo dnf install gmp-devel mpfr-devel libmpc-devel
```

### Issue: "makeinfo: command not found"

**Solution:** Install texinfo:
```bash
sudo apt-get install texinfo
```

### Issue: "flex: command not found"

**Solution:** Install flex:
```bash
sudo apt-get install flex
```

### Issue: "Permission denied" when creating /opt directories

**Solution:** Either:
1. Run with sudo (recommended)
2. Change PREFIX to $HOME in scripts:
   ```bash
   export XINIM_PREFIX=$HOME/xinim-toolchain
   export XINIM_SYSROOT=$HOME/xinim-sysroot
   ```

### Issue: "Out of memory" during GCC build

**Solution:** Reduce parallel jobs:
```bash
# Instead of make -j$(nproc)
make -j2  # Only use 2 cores
```

Or add swap space:
```bash
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Issue: Build fails with "cannot compute suffix of object files"

**Solution:** This usually means Stage 1 GCC wasn't built correctly. Try:
```bash
# Clean and rebuild
cd ~/xinim-sources/build-gcc-stage1
make distclean
cd /xinim/scripts
./build_gcc_stage1.sh
```

---

## Continuous Integration

### GitHub Actions Example

```yaml
name: Build XINIM Toolchain

on: [push, pull_request]

jobs:
  build-toolchain:
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential bison flex libgmp-dev libmpfr-dev \
            libmpc-dev texinfo help2man gawk libc6-dev git \
            wget tar xz-utils

      - name: Build toolchain
        run: |
          cd scripts
          ./setup_build_environment.sh
          ./build_binutils.sh
          ./build_gcc_stage1.sh
          ./build_dietlibc.sh
          ./build_gcc_stage2.sh

      - name: Validate toolchain
        run: |
          source /opt/xinim-toolchain/xinim-env.sh
          ./scripts/validate_toolchain.sh

      - name: Archive toolchain
        uses: actions/upload-artifact@v3
        with:
          name: xinim-toolchain
          path: /opt/xinim-toolchain
```

---

## Summary

**Minimum required packages:** 23 packages
**Recommended installation method:** Ubuntu/Debian with apt-get
**Alternative:** Docker container for clean environment
**Build time:** 1-4 hours depending on CPU
**Disk space:** 5-10 GB free space required

**Next steps after installing dependencies:**
1. Run `scripts/setup_build_environment.sh`
2. Follow Week 1 Implementation Guide
3. Validate with `scripts/validate_toolchain.sh`

**Validation:** All dependencies are checked by `setup_build_environment.sh` before beginning build.

---

**Document Status:** ✅ APPROVED
**Maintainer:** XINIM Toolchain Team
**Last Updated:** 2025-11-17
