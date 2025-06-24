# XINIM

XINIM is a modern C++23 reimplementation of the classic MINIX 1 operating system. It bundles the kernel, memory manager, file system and userland utilities into one educational codebase.

Consult [docs/BUILDING.md](docs/BUILDING.md) for the detailed build and testing guide.

## Quick start

1. **Install dependencies** on Ubuntu:
   ```bash
   sudo apt-get update
   sudo apt-get install -y <packages from docs/TOOL_INSTALL.md>
   ```
   The full list of required packages is maintained in [docs/TOOL_INSTALL.md](docs/TOOL_INSTALL.md).
2. **Build** using Clang (default):
   ```bash
   cmake -B build
   cmake --build build
   ```
   Use `BUILD_MODE=release` or `-DCMAKE_BUILD_TYPE=Release` for optimized builds.

## Cross compilation

To produce a bare x86-64 image enable the cross‑compile options:
```bash
cmake -B build -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=x86_64-elf-
cmake --build build
```
The Makefiles accept `CROSS_PREFIX=x86_64-elf-` for the same effect.
