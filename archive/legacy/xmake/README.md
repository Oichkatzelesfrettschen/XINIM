# archive/legacy/xmake

This directory contains xmake build system artifacts from the pre-CMake era.

The project migrated to CMake + Conan (Clang 21, libc++, C++23) and no longer uses xmake.

Contents:
- `xmake.lua.bak` - original xmake top-level build config
- `examples/` - xmake example configurations
- `scripts/` - xmake-era build scripts

Do not use these for building. See `scripts/conan_install.sh` and `CMakeLists.txt`.
