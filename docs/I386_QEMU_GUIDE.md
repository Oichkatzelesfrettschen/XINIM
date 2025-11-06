# XINIM i386 QEMU Guide

This guide provides comprehensive instructions for building and running XINIM on 32-bit i386 architecture using QEMU virtualization.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Building for i386](#building-for-i386)
3. [Running in QEMU](#running-in-qemu)
4. [Docker Environment](#docker-environment)
5. [QEMU Configuration Options](#qemu-configuration-options)
6. [Debugging](#debugging)
7. [Performance Tuning](#performance-tuning)
8. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Host System Requirements
- Linux-based system (Ubuntu 20.04+ recommended)
- Docker (optional, for containerized builds)
- At least 2GB RAM
- 5GB free disk space

### Required Packages

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    clang-18 \
    clang++-18 \
    lld-18 \
    gcc-multilib \
    g++-multilib \
    libc6-dev-i386 \
    qemu-system-x86 \
    qemu-utils \
    libsodium-dev \
    cmake \
    ninja-build
```

#### Fedora/RHEL
```bash
sudo dnf install -y \
    @development-tools \
    clang \
    lld \
    glibc-devel.i686 \
    libstdc++-devel.i686 \
    qemu-system-x86 \
    libsodium-devel \
    cmake \
    ninja-build
```

### Install xmake Build System
```bash
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile
```

---

## Building for i386

### Method 1: Native Build (with multilib support)

1. **Configure for i386 architecture:**
```bash
cd /path/to/XINIM
xmake config --arch=i386 --mode=release --toolchain=clang
```

2. **Build the kernel:**
```bash
xmake build
```

3. **Verify the build:**
```bash
file build/xinim
# Should output: build/xinim: ELF 32-bit LSB executable, Intel 80386, ...
```

### Method 2: Docker Build (Recommended)

The Docker method provides a consistent, isolated build environment with all dependencies pre-configured.

1. **Build the Docker image:**
```bash
docker-compose -f docker-compose.i386.yml build
```

2. **Start the development container:**
```bash
docker-compose -f docker-compose.i386.yml up -d
docker exec -it xinim-i386-dev /bin/bash
```

3. **Inside the container:**
```bash
xmake config --arch=i386
xmake build
```

### Method 3: Manual Cross-Compilation

For advanced users who want fine-grained control:

```bash
# Set environment variables
export CC=clang-18
export CXX=clang++-18
export CFLAGS="-m32 -march=i686 -mtune=generic"
export CXXFLAGS="-m32 -std=c++23 -march=i686 -mtune=generic"
export LDFLAGS="-m32"

# Configure and build
xmake config --arch=i386 --cc=$CC --cxx=$CXX
xmake build
```

---

## Running in QEMU

### Quick Start

The simplest way to run XINIM in QEMU:

```bash
./scripts/qemu_i386.sh
```

### Basic Usage

```bash
# Run with default settings (256MB RAM, qemu32 CPU)
./scripts/qemu_i386.sh

# Run with more memory
./scripts/qemu_i386.sh -m 512M

# Run with different CPU emulation
./scripts/qemu_i386.sh --cpu pentium3

# Run with graphical display
./scripts/qemu_i386.sh --display
```

### Advanced Configuration

```bash
# Custom kernel image location
./scripts/qemu_i386.sh -k /path/to/custom/kernel

# Use Q35 chipset (modern)
./scripts/qemu_i386.sh --machine q35

# Combine multiple options
./scripts/qemu_i386.sh -m 1G --cpu pentium3 --display --cmdline "debug=1"
```

---

## Docker Environment

### Building and Running in Docker

The Docker setup provides KVM acceleration support and a complete development environment.

#### Start Development Container
```bash
docker-compose -f docker-compose.i386.yml up -d
```

#### Access Container Shell
```bash
docker exec -it xinim-i386-dev /bin/bash
```

#### Build and Test Inside Container
```bash
# Configure
xmake config --arch=i386

# Build
xmake build

# Run in QEMU
./scripts/qemu_i386.sh
```

#### Stop Container
```bash
docker-compose -f docker-compose.i386.yml down
```

#### Clean Build Volumes
```bash
docker-compose -f docker-compose.i386.yml down -v
```

---

## QEMU Configuration Options

### CPU Types

Different CPU models provide different feature sets:

| CPU Type | Description | Use Case |
|----------|-------------|----------|
| `qemu32` | Generic 32-bit x86 | Maximum compatibility (default) |
| `486` | Intel 486 | Minimal features, legacy testing |
| `pentium` | Intel Pentium | MMX support |
| `pentium2` | Intel Pentium II | MMX, SSE |
| `pentium3` | Intel Pentium III | MMX, SSE, SSE2 |

Example:
```bash
./scripts/qemu_i386.sh --cpu pentium3
```

### Machine Types

| Machine | Description | Compatibility |
|---------|-------------|---------------|
| `pc` | Standard PC (i440FX) | Best compatibility (default) |
| `isapc` | ISA-only PC | Very minimal, no PCI |
| `q35` | Modern PC (Q35) | Latest features, may not work with all i386 code |

Example:
```bash
./scripts/qemu_i386.sh --machine pc
```

### Memory Configuration

```bash
# 256MB (default)
./scripts/qemu_i386.sh -m 256M

# 512MB
./scripts/qemu_i386.sh -m 512M

# 1GB (maximum recommended for i386)
./scripts/qemu_i386.sh -m 1G
```

### Acceleration

QEMU will automatically use KVM acceleration if available:

```bash
# Check KVM availability
ls -la /dev/kvm

# Ensure your user has access
sudo usermod -aG kvm $USER
```

If KVM is not available, QEMU falls back to TCG (software emulation).

---

## Debugging

### GDB Debugging

1. **Start QEMU in debug mode:**
```bash
./scripts/qemu_i386.sh -g
```

This starts QEMU with:
- CPU frozen at boot (`-S`)
- GDB server on port 1234 (`-s`)

2. **Connect with GDB:**
```bash
gdb build/xinim -ex 'target remote localhost:1234'
```

3. **Common GDB commands:**
```gdb
# Set breakpoint at function
break _start
break main

# Continue execution
continue

# Step instruction
stepi

# Step over function
nexti

# Display registers
info registers

# Display memory
x/10i $eip    # Show 10 instructions at current EIP
x/20wx $esp   # Show 20 words at current stack pointer

# Examine variables
print variable_name

# List source
list

# Quit
quit
```

### Custom GDB Port

```bash
./scripts/qemu_i386.sh -g --gdb-port 5555
gdb build/xinim -ex 'target remote localhost:5555'
```

### Serial Output Debugging

QEMU serial output is redirected to stdio by default. To save to file:

Modify the QEMU command:
```bash
qemu-system-i386 ... -serial file:/tmp/serial.log
```

---

## Performance Tuning

### KVM Acceleration

For best performance, use KVM when available:

```bash
# Check if KVM is loaded
lsmod | grep kvm

# Load KVM module (if needed)
sudo modprobe kvm-intel  # For Intel CPUs
# or
sudo modprobe kvm-amd    # For AMD CPUs
```

### CPU Optimization

Choose CPU type based on your needs:
- **Development/Testing**: `qemu32` (maximum compatibility)
- **Performance**: `pentium3` (more features, faster)

### Memory Settings

- **Minimum**: 128MB (may be slow)
- **Recommended**: 256MB (default)
- **Optimal**: 512MB - 1GB
- **Maximum**: 2GB (i386 PAE limit)

---

## Troubleshooting

### Build Errors

#### Error: Cannot find i386 libraries
```bash
# Install 32-bit development libraries
sudo apt-get install gcc-multilib g++-multilib libc6-dev-i386
```

#### Error: C++23 features not supported
```bash
# Ensure Clang 18+ is installed
clang++ --version
sudo apt-get install clang-18 clang++-18
```

### QEMU Errors

#### Error: Could not access KVM kernel module
```bash
# Add user to kvm group
sudo usermod -aG kvm $USER
# Log out and log back in
```

#### Error: Kernel image not found
```bash
# Build the kernel first
xmake build --arch=i386

# Or specify kernel path
./scripts/qemu_i386.sh -k /path/to/kernel
```

#### Error: QEMU crashes immediately
```bash
# Try with software emulation only
qemu-system-i386 -accel tcg -kernel build/xinim

# Or use ISA-only machine
./scripts/qemu_i386.sh --machine isapc
```

### Runtime Issues

#### Kernel doesn't boot
1. Check multiboot header is present:
   ```bash
   objdump -h build/xinim | grep multiboot
   ```

2. Verify ELF format:
   ```bash
   file build/xinim
   # Should be: ELF 32-bit LSB executable, Intel 80386
   ```

3. Try debug mode:
   ```bash
   ./scripts/qemu_i386.sh -g
   gdb build/xinim -ex 'target remote localhost:1234'
   ```

#### Serial output not visible
```bash
# Ensure -nographic flag is used (default in script)
./scripts/qemu_i386.sh
```

---

## Optimal Settings Summary

### For Development
```bash
./scripts/qemu_i386.sh \
    -m 512M \
    --cpu qemu32 \
    --machine pc
```

### For Performance Testing
```bash
./scripts/qemu_i386.sh \
    -m 1G \
    --cpu pentium3 \
    --machine pc
```

### For Debugging
```bash
./scripts/qemu_i386.sh \
    -m 256M \
    --cpu qemu32 \
    --machine pc \
    -g
```

### For Maximum Compatibility
```bash
./scripts/qemu_i386.sh \
    -m 256M \
    --cpu 486 \
    --machine isapc
```

---

## Additional Resources

- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [i386 Architecture Reference](https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-manual-325462.html)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [XINIM GitHub Repository](https://github.com/Oichkatzelesfrettschen/XINIM)

---

## Quick Reference

```bash
# Build for i386
xmake config --arch=i386 && xmake build

# Run in QEMU (basic)
./scripts/qemu_i386.sh

# Run in QEMU (debug)
./scripts/qemu_i386.sh -g

# Run in Docker
docker-compose -f docker-compose.i386.yml up -d
docker exec -it xinim-i386-dev /bin/bash

# Help
./scripts/qemu_i386.sh --help
```
