# Build Instructions

This document describes the current CMake + Conan build flow.

## Prerequisites
- CMake 3.28+
- Conan 2.0+
- Clang 18+ (preferred)
- Ninja (recommended generator)

See `docs/REQUIREMENTS.md` for full tooling details.

## Configure and Build
```bash
# Install dependencies and generate toolchain
scripts/conan_install.sh build Debug

# Configure and build
cmake --preset debug
cmake --build --preset debug
```

## Tests
```bash
ctest --output-on-failure --test-dir build
```

## Documentation
```bash
doxygen docs/Doxyfile
sphinx-build -b html docs/sphinx docs/sphinx/html
```
