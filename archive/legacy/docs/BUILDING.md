# Building and Testing

This document describes the current build flow using CMake + Conan.
See `docs/REQUIREMENTS.md` for full dependency lists.

## 1. Prerequisites
- CMake 3.28+
- Conan 2.0+
- Clang 18+ (preferred)
- Ninja (recommended)
- libsodium via Conan

## 2. Configure and Build
```sh
scripts/conan_install.sh build Debug
cmake --preset debug
cmake --build --preset debug
```

## 3. Testing
```sh
ctest --output-on-failure --test-dir build
```

## 4. Documentation
```sh
doxygen docs/Doxyfile
sphinx-build -b html docs/sphinx docs/sphinx/html
```

## 5. QEMU Boot
```sh
scripts/qemu_x86_64.sh -k build/xinim
```

## 6. Notes
- Warnings are treated as errors by default.
- All code changes must follow C++23 and Doxygen conventions.
- Use CMake presets for consistent configuration.
