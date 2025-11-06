# XINIM x86_64 QEMU Validation Guide

This document provides comprehensive instructions for validating XINIM on x86_64 architecture using QEMU virtualization within Docker containers.

## Overview

XINIM is a pure C++23 implementation of MINIX focusing exclusively on **modern 64-bit x86_64 architecture**. This guide covers:
- Building XINIM for x86_64
- Running in QEMU with optimal settings
- Using Docker for consistent build environment
- Debugging and performance tuning

---

## Architecture Focus

**XINIM targets x86_64 exclusively:**
- 64-bit Intel and AMD processors
- Modern CPU features (AVX2, AVX512)
- QEMU Q35 machine type with PCIe support
- Multiboot2/Limine boot protocol

---

## Prerequisites

### Host System
- Linux-based system (Ubuntu 20.04+ recommended)
- Docker (optional, for containerized builds)
- At least 4GB RAM
- 10GB free disk space

### Required Packages

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    clang-18 \
    clang++-18 \
    lld-18 \
    qemu-system-x86 \
    qemu-utils \
    libsodium-dev \
    cmake \
    ninja-build \
    git
```

#### Fedora/RHEL
```bash
sudo dnf install -y \
    @development-tools \
    clang \
    lld \
    qemu-system-x86 \
    libsodium-devel \
    cmake \
    ninja-build \
    git
```

### Install xmake
```bash
curl -fsSL https://xmake.io/shget.text | bash
source ~/.xmake/profile
```

---

## Building for x86_64

### Standard Build

```bash
cd /path/to/XINIM

# Configure for x86_64 (default architecture)
xmake config --mode=release --toolchain=clang

# Build the kernel
xmake build

# Verify the build
file build/xinim
# Expected: build/xinim: ELF 64-bit LSB executable, x86-64, ...
```

### Development Build with Optimizations

```bash
# Configure with debug symbols and optimizations
xmake config --mode=debug --toolchain=clang --ccache=y

# Build with verbose output
xmake build --verbose

# Run tests
xmake run test-all
```

### Performance Build

```bash
# Configure for maximum performance
xmake config --mode=release --toolchain=clang

# Build with link-time optimization
xmake build -j$(nproc)
```

---

## Running in QEMU

### Quick Start

```bash
./scripts/qemu_x86_64.sh
```

This launches QEMU with:
- 512MB RAM
- 2 CPUs
- Q35 machine type (modern PCIe)
- qemu64 CPU (generic 64-bit)
- KVM acceleration (if available)

### Common Usage

```bash
# Run with more resources
./scripts/qemu_x86_64.sh -m 2G --smp 4

# Run with modern CPU features (AVX2, AVX512)
./scripts/qemu_x86_64.sh --cpu Skylake-Client

# Run with host CPU passthrough (best performance, requires KVM)
./scripts/qemu_x86_64.sh --cpu host -m 4G --smp 8

# Run with graphical display
./scripts/qemu_x86_64.sh --display
```

---

## Docker Build Environment

### Dockerfile for x86_64

Create `.devcontainer/Dockerfile.x86_64`:

```dockerfile
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    clang-18 \
    clang++-18 \
    lld-18 \
    qemu-system-x86 \
    qemu-utils \
    libsodium-dev \
    cmake \
    ninja-build \
    git \
    curl \
    wget \
    gdb \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Set up Clang as default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100

# Install xmake
RUN cd /tmp && \
    wget https://github.com/xmake-io/xmake/releases/download/v2.8.7/xmake-v2.8.7.gz.run && \
    chmod +x xmake-v2.8.7.gz.run && \
    ./xmake-v2.8.7.gz.run --prefix=/usr/local && \
    rm xmake-v2.8.7.gz.run

WORKDIR /workspace

ENV CC=clang-18
ENV CXX=clang++-18

CMD ["/bin/bash"]
```

### Docker Compose Configuration

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  xinim-x86_64:
    build:
      context: .devcontainer
      dockerfile: Dockerfile.x86_64
    container_name: xinim-dev
    volumes:
      - .:/workspace:cached
      - xinim-build:/workspace/build
    working_dir: /workspace
    stdin_open: true
    tty: true
    privileged: true
    devices:
      - /dev/kvm:/dev/kvm
    cap_add:
      - SYS_ADMIN
    security_opt:
      - apparmor:unconfined

volumes:
  xinim-build:
```

### Using Docker

```bash
# Build container
docker-compose build

# Start container
docker-compose up -d

# Enter container
docker exec -it xinim-dev /bin/bash

# Inside container
xmake build
./scripts/qemu_x86_64.sh
```

---

## QEMU Configuration Options

### CPU Types

| CPU Type | Description | Features |
|----------|-------------|----------|
| `qemu64` | Generic 64-bit | Basic x86_64 (default) |
| `host` | Host passthrough | All host CPU features (requires KVM) |
| `Nehalem` | Intel Core i7 (2008) | SSE4.2 |
| `SandyBridge` | Intel (2011) | AVX |
| `IvyBridge` | Intel (2012) | AVX, F16C |
| `Haswell` | Intel (2013) | AVX2, FMA3 |
| `Broadwell` | Intel (2014) | AVX2 |
| `Skylake-Client` | Intel (2015) | AVX2, ADX, MPX |
| `Cascadelake-Server` | Intel (2019) | AVX512 |

**Recommendation**: Use `Skylake-Client` for modern features without AVX512 complexity.

### Machine Types

| Machine | Description | Use Case |
|---------|-------------|----------|
| `q35` | Modern PC (Q35, PCIe) | **Recommended** - Modern hardware |
| `pc` | Legacy PC (i440FX, PCI) | Compatibility mode |

### Memory Configuration

```bash
# Minimum for development
./scripts/qemu_x86_64.sh -m 512M

# Recommended
./scripts/qemu_x86_64.sh -m 2G

# Large workloads
./scripts/qemu_x86_64.sh -m 4G

# Maximum (adjust based on host)
./scripts/qemu_x86_64.sh -m 8G
```

### SMP Configuration

```bash
# Single CPU (debugging)
./scripts/qemu_x86_64.sh --smp 1

# Default (2 CPUs)
./scripts/qemu_x86_64.sh --smp 2

# Multi-core (4 CPUs)
./scripts/qemu_x86_64.sh --smp 4

# High performance (8 CPUs)
./scripts/qemu_x86_64.sh --smp 8
```

---

## Debugging

### GDB Setup

1. **Start QEMU in debug mode:**
```bash
./scripts/qemu_x86_64.sh -g
```

2. **Connect with GDB:**
```bash
gdb build/xinim -ex 'target remote localhost:1234'
```

3. **Essential GDB commands:**
```gdb
# Breakpoints
break _start
break main
break kernel_main

# Execution control
continue
step
next
finish

# Inspection
info registers
info frame
info threads
backtrace

# Memory examination
x/20i $rip          # 20 instructions at RIP
x/32gx $rsp         # 32 64-bit words at stack
x/s 0x<address>     # String at address

# Watchpoints
watch variable_name
rwatch variable_name  # Read watchpoint

# Display
display $rax
display $rip
```

### QEMU Monitor

Access QEMU monitor with `Ctrl+A, C`:

```
# Common monitor commands
info registers       # Show CPU registers
info mem            # Show memory mappings
info tlb            # Show TLB entries
info mtree          # Show memory tree
system_reset        # Reset the system
quit                # Exit QEMU
```

---

## Performance Tuning

### KVM Acceleration

Ensure KVM is available:

```bash
# Check KVM module
lsmod | grep kvm

# Load if needed
sudo modprobe kvm-intel  # Intel
sudo modprobe kvm-amd    # AMD

# Verify access
ls -la /dev/kvm
sudo usermod -aG kvm $USER
# Log out and back in
```

### Optimal Settings for Development

```bash
./scripts/qemu_x86_64.sh \
    --cpu Skylake-Client \
    -m 2G \
    --smp 4 \
    --machine q35
```

### Optimal Settings for Performance Testing

```bash
./scripts/qemu_x86_64.sh \
    --cpu host \
    -m 4G \
    --smp 8 \
    --machine q35
```

### Optimal Settings for Debugging

```bash
./scripts/qemu_x86_64.sh \
    --cpu qemu64 \
    -m 1G \
    --smp 1 \
    --machine q35 \
    -g
```

---

## Validation Checklist

### Build Validation

- [ ] Clean build succeeds: `xmake clean && xmake build`
- [ ] Binary is ELF 64-bit x86-64: `file build/xinim`
- [ ] No compilation warnings in release mode
- [ ] All tests pass: `xmake run test-all`

### QEMU Boot Validation

- [ ] Boots successfully with default settings
- [ ] Serial output visible
- [ ] No kernel panics
- [ ] Multiboot header detected by QEMU

### Feature Validation

- [ ] HAL initialization succeeds
- [ ] CPUID detection works
- [ ] Interrupt handling functional
- [ ] Memory management operational
- [ ] Scheduler running

### Performance Validation

- [ ] KVM acceleration active (check boot messages)
- [ ] Boot time acceptable (< 5 seconds to kernel)
- [ ] No excessive CPU usage on host
- [ ] Memory usage stable

---

## Troubleshooting

### Build Issues

**Error: C++23 not supported**
```bash
clang++ --version  # Should be 18+
sudo apt-get install clang-18 clang++-18
```

**Error: libsodium not found**
```bash
sudo apt-get install libsodium-dev
```

### QEMU Issues

**Error: KVM not available**
```bash
# Check if KVM module is loaded
lsmod | grep kvm

# Check permissions
sudo usermod -aG kvm $USER
# Log out and back in
```

**Error: Kernel doesn't boot**
```bash
# Verify ELF format
file build/xinim

# Check multiboot header
objdump -h build/xinim | grep multiboot

# Try debug mode
./scripts/qemu_x86_64.sh -g
```

**Error: No serial output**
```bash
# Ensure -nographic is used (default in script)
# Check kernel console initialization
```

---

## Quick Reference

```bash
# Build
xmake config --toolchain=clang && xmake build

# Run (basic)
./scripts/qemu_x86_64.sh

# Run (optimized)
./scripts/qemu_x86_64.sh --cpu host -m 4G --smp 8

# Debug
./scripts/qemu_x86_64.sh -g
gdb build/xinim -ex 'target remote :1234'

# Docker
docker-compose up -d
docker exec -it xinim-dev /bin/bash
```

---

## Summary

XINIM is validated for x86_64 architecture with:
✅ Pure C++23 implementation
✅ Modern CPU feature support (AVX2/AVX512)
✅ QEMU virtualization with KVM acceleration
✅ Comprehensive debugging support
✅ Docker containerization
✅ Focused single-architecture design

For more information, see:
- [Architecture Documentation](ARCHITECTURE_HAL.md)
- [Hardware Matrix](HARDWARE_MATRIX.md)
- [Build Instructions](BUILDING.md)
