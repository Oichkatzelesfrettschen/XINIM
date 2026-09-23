# Linting and Formatting Strategy

Version: 0.2
Date: 2025-12-31
Status: Draft (active)

## Goals

- Enforce C++23 for project-owned implementations while retaining documented
  C and assembly ABI boundaries and pinned upstream sources.
- Treat all warnings as errors in CI and local builds.
- Keep formatting deterministic and automated.

## Required Tools

- clang-format (formatting)
- clang-tidy (semantic checks)
- cppcheck (static analysis)

## Optional Tools

- IWYU (include hygiene)
- lizard (complexity)
- flawfinder (security scanning)

## Configuration

- `.clang-format` defines the repository style and targets C++23.
- `.clang-tidy` enables the documented freestanding semantic profile with
  `WarningsAsErrors: '*'`; `test/.clang-tidy` adds hosted test checks.
- CMake generates lane-specific `compile_commands.json` files for clang-tidy.
- `docs/analysis/CLANG_TIDY_PROFILE.md` records check boundaries, exceptions,
  compiled-source coverage, and the separate userspace diagnostic baseline.

## Standard Commands

- Format reviewed diagnostic ranges with Clang 22 `clang-format` or
  `clang-apply-replacements --format --style=file`.
- Lint: `clang-tidy -p build/i486/Debug <compiled-source.cpp>`.
- Cppcheck: `cppcheck --enable=all --inconclusive --std=c++23`.

## Policy

- Changed C++ sources and compiled consumers of changed headers must pass the
  applicable warning-as-error checks in the CI lane.
- Review formatter replacements at the reported ranges before applying them;
  whole-file formatting can rewrite unrelated code and ABI headers.
- New applicable diagnostics require source fixes. A checker exception requires
  a documented boundary and review obligation.
- Migrate touched project-owned C implementations to C++23. Preserve genuine
  C, assembly, firmware, and third-party boundaries.
