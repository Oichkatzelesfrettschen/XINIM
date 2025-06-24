# XINIM

XINIM is a modern C++23 reimagining of MINIX 1. The project builds with CMake or traditional Makefiles on both x86-64 and arm64 hosts.

## Building

```bash
cmake -B build
cmake --build build
```

Refer to `docs/BUILDING.md` for detailed options such as cross-compilation and driver selection.

## Cleaning the Workspace

Use the provided helper to remove all build artifacts:

```bash
./tools/clean.sh
```

This script deletes the `build/` directory produced by CMake as well as `tools/build`, ensuring that subsequent builds start from a pristine state.

