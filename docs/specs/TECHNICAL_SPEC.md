# Xinim Technical Specification

Version: 0.1
Date: 2025-12-31
Status: Draft (active)

## Purpose
Define the technical architecture, build system, dependency management, and
migration strategy required to standardize Xinim on pure C++23 and CMake.

## Build System
- CMake is the authoritative build system for all targets.
- CMake presets define Debug/Release/RelWithDebInfo and toolchain selection.
- Conan generates a CMake toolchain file and dependency targets.
- Warnings-as-errors are enforced via target compile options.

## Toolchain
- Preferred compiler: Clang 18+.
- Alternate compiler: GCC 13+.
- Linker: lld (preferred) or system linker.
- C++ standard: ISO C++23 only.
## Target Layout (CMake)
- kernel: core microkernel and arch-specific components.
- servers: VFS, process manager, memory manager, reincarnation server.
- userland: shell, commands, utilities.
- libc: refactored C++23 runtime and compatibility layer.
- tests: unit, integration, and staged POSIX-progress validation.
- third_party: Conan-managed or modernized vendored deps.

Each target exposes:
- target_include_directories (PRIVATE/PUBLIC as needed)
- target_compile_features (cxx_std_23)
- target_compile_options (warnings-as-errors)
- target_link_libraries (explicit dependency graph)
## Dependency Management (Conan)
- conanfile.py declares external dependencies (libsodium, limine, etc.).
- Profiles define compiler, libc++, and C++23 settings.
- `conan install` generates a toolchain file and CMake presets integration.
- third_party sources are either:
  - replaced by Conan packages, or
  - modernized and isolated under a dedicated CMake target.

## Build Outputs
- build/ for CMake outputs (ignored by git).
- dist/ for bootable images and artifacts (ignored by git).
- docs/doxygen and docs/sphinx/html for generated docs.
## C++23 Migration Strategy
- Phase A: Convert build system and dependency graph (CMake + Conan).
- Phase B: Convert C translation units to C++23, preserving ABI expectations.
- Phase C: Replace legacy third_party code with modern equivalents where viable.
- Phase D: Enforce documentation and formatting on all modified sources.

## Documentation
- Doxygen generates XML for all public headers.
- Sphinx/Breathe builds HTML documentation and API references.

## QEMU Validation
- x86_64 boot via `scripts/qemu_x86_64.sh`.
- Pentium boot via `scripts/run_qemu_pentium.sh`.
- Serial output captured to logs for debugging.
