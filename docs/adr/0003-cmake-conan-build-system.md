# ADR 0003: CMake + Conan as Sole Build System

Status: Accepted

## Context

Xinim previously used xmake as its build system. The project migrated to CMake + Conan
to align with the CachyOS/Arch toolchain (Clang 21, libc++, C++23) and to benefit from
CMake's wider ecosystem, Conan's package management, and CMakePresets.json for
reproducible debug/release builds.

## Decision

Use CMake 3.28+ with Ninja as the generator and Conan 2.x for dependency management.
The Conan profile at `conan/profiles/xinim-clang` pins the exact compiler, standard,
and runtime settings.

## Consequences

- xmake files are archived to `archive/legacy/xmake/`.
- `conanfile.py` is the package descriptor; no runtime Conan deps (crypto is vendored).
- CMakePresets.json provides `debug` and `release` presets with binaryDir under `build/`.
- `src/kernel/linker.ld` was merged into root `linker.ld` (Phase 2 cleanup).

## Date

2026-02-26
