# XINIM Build Scripts

This directory contains automated build scripts for the XINIM cross-compiler toolchain and development environment.

---

## Quick Start

```bash
# 1. Setup build environment
sudo ./setup_build_environment.sh

# 2. Source environment variables
source /opt/xinim-toolchain/xinim-env.sh

# 3. Build toolchain (in order)
./build_binutils.sh      # ~10 minutes
./build_gcc_stage1.sh    # ~30 minutes
./build_dietlibc.sh          # ~5 minutes
./build_gcc_stage2.sh    # ~60 minutes
```

**Total Time:** ~2 hours

---

## Scripts Overview

### 1. `setup_build_environment.sh` ⚙️

**Purpose:** Initialize build environment and download sources

**What it does:**
- Creates `/opt/xinim-toolchain` (toolchain installation)
- Creates `/opt/xinim-sysroot` (target system root)
- Creates `~/xinim-build` (build directories)
- Creates `~/xinim-sources` (source code)
- Downloads binutils, GCC, dietlibc, Linux headers
- Extracts all sources
- Downloads GCC prerequisites (GMP, MPFR, MPC, ISL)
- Generates environment script

**Usage:**
```bash
sudo ./setup_build_environment.sh
```

**Prerequisites:**
- Ubuntu 20.04+ or Debian 11+
- ~50 GB free disk space
- Internet connection
- sudo privileges

**Output:**
- `/opt/xinim-toolchain/` - Toolchain installation prefix
- `/opt/xinim-sysroot/` - System root
- `~/xinim-build/` - Build directories
- `~/xinim-sources/` - Source code (~500 MB)
- `/opt/xinim-toolchain/xinim-env.sh` - Environment script

**Time:** ~5-10 minutes (depending on internet speed)

---

### 2. `build_binutils.sh` 🔧

**Purpose:** Build binutils (assembler, linker, binary utilities)

**What it does:**
- Configures binutils 2.41 for `x86_64-xinim-elf`
- Builds with gold linker, LTO support, plugins
- Installs to `/opt/xinim-toolchain/bin/`
- Verifies installation

**Installed Tools:**
- `x86_64-xinim-elf-as` - Assembler
- `x86_64-xinim-elf-ld` - Linker
- `x86_64-xinim-elf-ar` - Archiver
- `x86_64-xinim-elf-nm` - Symbol table viewer
- `x86_64-xinim-elf-objcopy` - Object file converter
- `x86_64-xinim-elf-objdump` - Object file disassembler
- `x86_64-xinim-elf-ranlib` - Archive index generator
- `x86_64-xinim-elf-readelf` - ELF file inspector
- `x86_64-xinim-elf-size` - Section size viewer
- `x86_64-xinim-elf-strings` - String extractor
- `x86_64-xinim-elf-strip` - Symbol stripper

**Usage:**
```bash
source /opt/xinim-toolchain/xinim-env.sh
./build_binutils.sh
```

**Prerequisites:**
- `setup_build_environment.sh` must be run first

**Time:** ~5-10 minutes

**Disk Space:** ~150 MB installed, ~500 MB build directory

---

### 3. `build_gcc_stage1.sh` 🏗️

**Purpose:** Build GCC Stage 1 (bootstrap compiler without libc)

**What it does:**
- Configures GCC 13.2.0 without libc support
- Builds C and C++ compilers (freestanding only)
- Builds libgcc (compiler support library)
- Installs to `/opt/xinim-toolchain/bin/`
- Verifies compilation of freestanding code

**Installed Compilers:**
- `x86_64-xinim-elf-gcc` - C compiler
- `x86_64-xinim-elf-g++` - C++ compiler
- `x86_64-xinim-elf-cpp` - C preprocessor

**Capabilities:**
- ✅ Compile freestanding C/C++ (kernel, bootloader)
- ✅ Inline assembly
- ✅ C++23 features (no standard library)
- ❌ Cannot use standard library (printf, malloc, etc.)

**Usage:**
```bash
source /opt/xinim-toolchain/xinim-env.sh
./build_gcc_stage1.sh
```

**Prerequisites:**
- `build_binutils.sh` must complete successfully

**Time:** ~20-30 minutes

**Disk Space:** ~500 MB installed, ~2 GB build directory

---

### 4. `build_dietlibc.sh` 📚

**Purpose:** Build dietlibc (C standard library)

**What it does:**
- Configures dietlibc 0.34 for x86_64
- Builds static libc.a
- Installs headers to `/opt/xinim-sysroot/usr/include/`
- Installs libraries to `/opt/xinim-sysroot/lib/`
- Creates XINIM syscall adapter
- Creates compatibility symlinks (lib64 → lib)

**Installed Components:**
- `libc.a` - C standard library (~1 MB)
- `libm.a` - Math library (linked into libc.a)
- `libpthread.a` - Threading library (linked into libc.a)
- `crt1.o, crti.o, crtn.o` - Startup files
- Headers: `stdio.h`, `stdlib.h`, `unistd.h`, `pthread.h`, etc.

**Usage:**
```bash
source /opt/xinim-toolchain/xinim-env.sh
./build_dietlibc.sh
```

**Prerequisites:**
- `build_gcc_stage1.sh` must complete successfully

**Time:** ~5 minutes

**Disk Space:** ~2 MB installed

---

### 5. `build_gcc_stage2.sh` 🎓

**Purpose:** Build GCC Stage 2 (full compiler with libc and libstdc++)

**What it does:**
- Configures GCC 13.2.0 with dietlibc
- Builds full C and C++ compilers
- Builds libstdc++ (C++ standard library)
- Enables POSIX threads, TLS, LTO
- Installs to `/opt/xinim-toolchain/bin/`

**Capabilities:**
- ✅ Full C17 support
- ✅ Full C++23 support
- ✅ Standard library (libc, libstdc++)
- ✅ POSIX threads
- ✅ Link-Time Optimization (LTO)
- ✅ Thread-local storage (TLS)

**Usage:**
```bash
source /opt/xinim-toolchain/xinim-env.sh
./build_gcc_stage2.sh
```

**Prerequisites:**
- `build_dietlibc.sh` must complete successfully

**Time:** ~30-60 minutes

**Disk Space:** ~2 GB installed

---

### 6. `qemu_x86_64.sh` 🖥️

**Purpose:** Launch XINIM in QEMU (x86_64 emulation)

**What it does:**
- Configures QEMU with optimal x86_64 settings
- Enables KVM acceleration (if available)
- Sets up VirtIO devices (network, block storage)
- Configures serial console
- Enables GDB debugging

**Usage:**
```bash
# Build XINIM kernel first
xmake build xinim

# Launch in QEMU
./qemu_x86_64.sh
```

**QEMU Configuration:**
- Machine: Q35 chipset with KVM acceleration
- CPU: Host passthrough (or core2duo)
- Memory: 4 GB
- Cores: 4 SMP
- Network: VirtIO-Net (TAP interface)
- Storage: VirtIO-Block
- Serial: stdio
- Debug: GDB server on port 1234

**Prerequisites:**
- QEMU installed: `sudo apt-get install qemu-system-x86`
- XINIM kernel built: `build/xinim`

---

### 7. `build.sh` (legacy)

**Purpose:** Original XINIM build script

**Note:** Use xmake or CMake instead for modern builds.

---

## Environment Variables

After running `setup_build_environment.sh`, source the environment:

```bash
source /opt/xinim-toolchain/xinim-env.sh
```

**Variables set:**
- `XINIM_PREFIX=/opt/xinim-toolchain` - Toolchain installation
- `XINIM_SYSROOT=/opt/xinim-sysroot` - System root
- `XINIM_TARGET=x86_64-xinim-elf` - Target triplet
- `XINIM_BUILD_DIR=~/xinim-build` - Build directories
- `XINIM_SOURCES_DIR=~/xinim-sources` - Source code
- `PATH` - Adds toolchain to PATH
- `CC, CXX, AR, LD, etc.` - Cross-compiler tool variables

**Add to `.bashrc` for persistence:**
```bash
echo 'source /opt/xinim-toolchain/xinim-env.sh' >> ~/.bashrc
```

---

## Build Order

**IMPORTANT:** Build scripts must be run in this exact order:

```
1. setup_build_environment.sh  (setup)
2. build_binutils.sh           (assembler, linker)
3. build_gcc_stage1.sh         (bootstrap compiler)
4. build_dietlibc.sh               (C library)
5. build_gcc_stage2.sh         (full compiler)
```

**Dependencies:**
```
setup_build_environment.sh
    ↓
build_binutils.sh
    ↓
build_gcc_stage1.sh
    ↓
build_dietlibc.sh
    ↓
build_gcc_stage2.sh
    ↓
(Toolchain complete - ready to build XINIM)
```

---

## Verification

After building, verify toolchain:

```bash
# Check all tools exist
ls /opt/xinim-toolchain/bin/x86_64-xinim-elf-*

# Check versions
x86_64-xinim-elf-gcc --version
x86_64-xinim-elf-ld --version

# Test compilation
echo 'int main() { return 0; }' > test.c
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot -static test.c -o test
./test && echo "Success!" || echo "Failed"
rm test.c test
```

---

## Disk Space Requirements

| Component | Build Size | Installed Size |
|-----------|------------|----------------|
| **Sources** | N/A | ~500 MB |
| **binutils** | ~500 MB | ~150 MB |
| **GCC Stage 1** | ~2 GB | ~500 MB |
| **dietlibc** | ~100 MB | ~2 MB |
| **GCC Stage 2** | ~3 GB | ~2 GB |
| **Total** | ~6 GB | ~3 GB |

**Cleanup:**
```bash
# Delete build directories (saves ~6 GB)
rm -rf ~/xinim-build/*

# Keep sources for potential rebuilds
# Don't delete: ~/xinim-sources/
```

---

## Troubleshooting

### Common Issues

**1. "Permission denied" on /opt**
```bash
# Solution: Run setup with sudo
sudo ./setup_build_environment.sh

# Or create directories with user ownership
sudo mkdir -p /opt/xinim-{toolchain,sysroot}
sudo chown -R $USER:$USER /opt/xinim-{toolchain,sysroot}
```

**2. "No space left on device"**
```bash
# Check disk space
df -h

# Clean build directories
rm -rf ~/xinim-build/*
```

**3. "configure: error: C compiler cannot create executables"**
```bash
# Install build tools
sudo apt-get install build-essential
```

**4. "makeinfo: command not found"**
```bash
# Install documentation tools
sudo apt-get install texinfo help2man
```

**5. GCC build fails - missing GMP/MPFR/MPC**
```bash
# Download GCC prerequisites
cd ~/xinim-sources/gcc-13.2.0
./contrib/download_prerequisites
```

### Performance Tips

**Use all CPU cores:**
```bash
# Scripts use make -j$(nproc) by default
# To explicitly set: export MAKEFLAGS="-j8"
```

**Use ccache for faster rebuilds:**
```bash
sudo apt-get install ccache
export PATH="/usr/lib/ccache:$PATH"
```

**Build on SSD (faster than HDD):**
```bash
# Use tmpfs (RAM disk) for builds
export XINIM_BUILD_DIR=/tmp/xinim-build
```

---

## Related Documentation

- [Week 1 Implementation Guide](/home/user/XINIM/docs/guides/WEEK1_IMPLEMENTATION_GUIDE.md) - Step-by-step instructions
- [Toolchain Specification](/home/user/XINIM/docs/specs/TOOLCHAIN_SPECIFICATION.md) - Technical details
- [SUSv4 Compliance Audit](/home/user/XINIM/docs/SUSV4_POSIX_2017_COMPLIANCE_AUDIT.md) - Full roadmap
- [XINIM Building Guide](/home/user/XINIM/docs/BUILDING.md) - General building instructions

---

## Support

- **Issues:** https://github.com/Oichkatzelesfrettschen/XINIM/issues
- **Documentation:** `/home/user/XINIM/docs/`
- **OSDev Wiki:** https://wiki.osdev.org/

---

**Last Updated:** 2025-11-17
**Toolchain Version:** binutils 2.41, GCC 13.2.0, dietlibc 0.34
**Maintainer:** XINIM Toolchain Team
