# XINIM Project

XINIM is a full C++23 reimplementation of the classic MINIX 1 operating system. The repository combines the kernel, memory management, file system, libraries, commands, and tooling into one cohesive environment. It demonstrates modern systems programming techniques with strong type safety, extensive error handling, and cross-platform support.

For a comprehensive guide with build instructions and architectural details, refer to [docs/README_renamed.md](docs/README_renamed.md).

**XINIM** is a clean-room C++ 23 re-implementation of the original MINIX 1 educational operating system.  
The repository contains the full kernel, memory manager, file-system, classic user-land utilities, and supporting build scripts.

---

## Table of Contents

1. [Prerequisites](#prerequisites)  
2. [Native Build (Quick Start)](#native-build-quick-start)  
3. [Cross-Compilation](#cross-compilation)  
4. [Cleaning the Workspace](#cleaning-the-workspace)  
5. [Documentation](#documentation)  
6. [License](#license)

---

## Prerequisites

Install the tool-chain and host packages listed in **`docs/TOOL_INSTALL.md`**.  
Example for Ubuntu 24.04 LTS:

```bash
sudo apt-get update
sudo apt-get install -y $(tr '\n' ' ' < docs/TOOL_INSTALL.md)
````

---

## Native Build (Quick Start)

```bash
# Configure (Debug build by default)
cmake -B build

# Compile everything
cmake --build build -j$(nproc)

# Boot the resulting disk image under QEMU
./tools/run_qemu.sh build/xinim.img
```

To build an optimised image add:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

---

## Cross-Compilation

Building a freestanding x86-64 image on any host:

```bash
cmake -B build \
      -DCROSS_COMPILE_X86_64=ON \
      -DCROSS_PREFIX=x86_64-elf-

cmake --build build
```

The legacy Makefiles accept the same triplet:

```bash
make CROSS_PREFIX=x86_64-elf- all
```

The generated `xinim.img` boots via GRUB 2, Limine, or directly with:

```bash
qemu-system-x86_64 -drive file=xinim.img,if=none,format=raw
```

For additional architectures (AArch64, RISC-V) see **`docs/BUILDING.md`**.

---

## Cleaning the Workspace

```bash
# Remove all build artefacts and test outputs
./tools/clean.sh
```

or, if using CMake directly:

```bash
cmake --build build --target clean
rm -rf build/
```

---

## Documentation

| Document                      | Description                         |
| ----------------------------- | ----------------------------------- |
| `docs/BUILDING.md`            | Full build and flashing guide       |
| `docs/ARCHITECTURE.md`        | Subsystem overview and design notes |
| `docs/TOOL_INSTALL.md`        | OS-specific dependency list         |
| `docs/sphinx/html/index.html` | Generated developer manual in HTML  |

---

## Sort Utility Merge Mode

The modern `sort` command supports a multi-file merge when invoked with the
`-m` flag. Each input file must already be sorted; the utility performs a
streaming k-way merge using the same comparison rules as regular sorting.  When
combined with the `-u` option, duplicate lines encountered across input files
are removed during the merge.

---

## License

Dual-licensed under **Apache-2.0** or **MIT**.  See `LICENSE` for details.

```
```
