# Xinim Agent Guidelines

This file supplements docs/AGENTS.md.

- Follow docs/AGENTS.md for toolchain and documentation rules.
- Maintain CMake + Conan as the sole build system.
- Treat warnings as errors and keep clang-format/clang-tidy configs in sync.
- Update documentation when build or dependency behavior changes.
- Keep generated artifacts out of git (build/, dist/, docs outputs).
