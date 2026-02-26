# Modernization Status
Date: 2025-12-31

## Summary
- Build system: CMake + Conan in progress (xmake deprecated).
- CMake: target-based warnings and includes added; presets + toolchain path.
- Docs: requirements/build/lint/test strategy updated to CMake + Conan.
- QEMU: scripts present and updated to CMake build messaging.

## Inventory Snapshot
- C sources: ~3135
- C++ sources: ~367
- Headers: ~581
- High C concentration: third_party/ and libc/ trees

## Known Gaps
- Full C to C++23 migration pending (pure C++23 requirement).
- Conan integration requires profile + `conan install` in build flow.
- Many subsystem stubs remain (kernel, VFS, servers, HAL, crypto, drivers).
- Documentation still references xmake in some legacy files.

## Next Actions
- Replace xmake references in docs/scripts.
- Define explicit command target list (remove file(GLOB)).
- Introduce Conan profiles + dependency manifest.
- Begin staged C to C++23 conversion with Doxygen coverage.
