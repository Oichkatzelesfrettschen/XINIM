# XINIM Container Infrastructure

This directory contains the lightweight container infrastructure for building, testing, debugging, and running CI pipelines for XINIM.

## Overview

The container infrastructure is designed to be:

- **Lightweight**: Multi-stage builds minimize image size
- **Compatible**: Works with both Docker and Podman
- **Comprehensive**: Supports building, testing, debugging, and CI workflows
- **Reproducible**: Consistent build environment across all systems

## Quick Start

### Prerequisites

- Docker or Podman installed
- Git (for cloning the repository)

### Build and Test

```bash
# Build the kernel in a container
./containers/container.sh build

# Run all tests in a container
./containers/container.sh test

# Start an interactive debugging session
./containers/container.sh debug
```

## Container Stages

The `Containerfile` uses multi-stage builds to create optimized images for different use cases:

### 1. `build-env` (Lightweight Build)

**Size**: ~1.5 GB  
**Purpose**: Building XINIM kernel

Includes:
- Clang 18 with C++23 support
- CMake & Ninja build systems
- xmake (primary build system)
- libsodium (crypto dependency)

```bash
./containers/container.sh build-image build
./containers/container.sh build
```

### 2. `test-env` (Testing)

**Size**: ~2.5 GB  
**Purpose**: Running tests and QEMU emulation

Includes everything in `build-env` plus:
- QEMU x86_64 for kernel testing
- GDB for debugging
- Valgrind for memory analysis
- lcov for coverage reports
- Python 3 with pytest

```bash
./containers/container.sh build-image test
./containers/container.sh test
```

### 3. `debug-env` (Full Debug)

**Size**: ~3 GB  
**Purpose**: Interactive debugging sessions

Includes everything in `test-env` plus:
- strace/ltrace for system call tracing
- Sanitizer libraries (ASan, TSan, UBSan)
- Pre-configured GDB for kernel debugging
- Vim for in-container editing

```bash
./containers/container.sh build-image debug
./containers/container.sh debug
```

### 4. `ci-env` (CI Pipeline)

**Size**: ~3 GB  
**Purpose**: GitHub Actions CI runs

Includes everything in `test-env` plus:
- Doxygen for documentation generation
- Graphviz for diagrams
- CI-specific environment variables

```bash
./containers/container.sh build-image ci
./containers/container.sh ci
```

### 5. `runtime` (Minimal)

**Size**: Varies based on built kernel (verify with `docker images` or `podman images`)  
**Purpose**: Running pre-built XINIM kernel

Minimal image containing only QEMU and the built kernel.

**Note**: This stage requires a pre-built kernel binary in the build context:
```bash
./containers/container.sh build
cp build/xinim ./xinim
docker build --target runtime -t xinim-runtime -f containers/Containerfile .
```

## Command Reference

### Building Images

```bash
# Build specific stage
./containers/container.sh build-image build   # Build environment
./containers/container.sh build-image test    # Test environment
./containers/container.sh build-image debug   # Debug environment
./containers/container.sh build-image ci      # CI environment

# Build without cache
./containers/container.sh --no-cache build-image build
```

### Building XINIM

```bash
# Standard build
./containers/container.sh build

# Force rebuild of container first
./containers/container.sh --rebuild build
```

### Testing

```bash
# Run all tests
./containers/container.sh test

# Run linting
./containers/container.sh lint

# Run static analysis
./containers/container.sh analyze

# Generate coverage report
./containers/container.sh coverage
```

### Debugging

```bash
# Interactive debug session
./containers/container.sh debug

# Boot in QEMU (basic)
./containers/container.sh qemu

# Boot in QEMU with GDB server
./containers/container.sh qemu-debug
# Then connect with: gdb build/xinim -ex 'target remote localhost:1234'
```

### Interactive Shell

```bash
# Shell in build environment
./containers/container.sh shell

# Full CI pipeline in container
./containers/container.sh ci
```

### Cleanup

```bash
# Remove all XINIM images
./containers/container.sh clean

# Prune unused container resources
./containers/container.sh prune
```

## Using Podman Instead of Docker

The container infrastructure automatically detects Podman if available. To force using a specific runtime:

```bash
./containers/container.sh --runtime podman build
./containers/container.sh --runtime docker test
```

## Rootless Containers

Both Docker and Podman support rootless operation:

```bash
# Podman (rootless by default)
podman build -t xinim-build:local -f containers/Containerfile --target build-env .

# Docker (requires setup)
docker context use rootless
```

## GitHub Actions Integration

The CI workflow (`.github/workflows/container-build.yml`) automatically:

1. Builds the CI container image
2. Caches builds using GitHub Actions cache
3. Runs tests in isolated containers
4. Performs static analysis
5. Checks code formatting
6. Runs QEMU boot smoke tests

### Manual Workflow Trigger

```bash
gh workflow run container-build.yml
```

## Debugging Kernel in Container

### Using GDB

```bash
# Terminal 1: Start QEMU with GDB server
./containers/container.sh qemu-debug

# Terminal 2: Connect GDB
gdb build/xinim -ex 'target remote localhost:1234'

# GDB commands
(gdb) break kernel_main
(gdb) continue
(gdb) info registers
(gdb) x/20i $rip
```

### Using AddressSanitizer

```bash
./containers/container.sh shell
# Inside container:
xmake config --mode=asan --toolchain=clang
xmake build xinim-asan
xmake run xinim-asan
```

### Using Valgrind

```bash
./containers/container.sh shell
# Inside container:
xmake build
valgrind --leak-check=full ./build/xinim
```

## Customizing the Container

### Adding Dependencies

Edit `containers/Containerfile` and add packages to the appropriate stage:

```dockerfile
# In build-env stage
RUN apt-get update && apt-get install -y --no-install-recommends \
    your-package-here \
    && rm -rf /var/lib/apt/lists/*
```

### Creating Custom Scripts

Create scripts in the container and mount them:

```bash
docker run -v ./my-script.sh:/xinim/my-script.sh xinim-build:local ./my-script.sh
```

## Troubleshooting

### "Permission denied" errors

With Podman or SELinux, use the `:Z` volume flag:

```bash
docker run -v /path:/container:Z image
```

### Image not building

Clear cache and rebuild:

```bash
./containers/container.sh --no-cache build-image build
```

### xmake not found

The xmake binary is at `/root/.local/bin/xmake`. Ensure PATH is set:

```bash
export PATH="/root/.local/bin:$PATH"
```

### QEMU not starting

Ensure the kernel is built first:

```bash
./containers/container.sh build
./containers/container.sh qemu
```

## Architecture Support

Currently, the containers support:

- **x86_64**: Full support (primary architecture)
- **ARM64**: Build support (cross-compilation possible)

## License

This container infrastructure is part of XINIM and is licensed under the BSD 3-Clause License.
