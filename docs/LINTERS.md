# Linting and Formatting Strategy

Version: 0.2
Date: 2025-12-31
Status: Draft (active)

## Goals
- Enforce C++23-only code across the entire tree.
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
- `.clang-tidy` enables modernize/readability/performance checks and sets
  WarningsAsErrors: `*`.
- CMake generates `compile_commands.json` for clang-tidy.

## Standard Commands
- Format: `clang-format -i <files>`
- Lint: `clang-tidy -p build <file>`
- Cppcheck: `cppcheck --enable=all --inconclusive --std=c++23`.

## Policy
- All modified C++ files must pass clang-format and clang-tidy.
- New diagnostics require fixes or explicit, documented suppression.
- Any C source files must be migrated to C++23 or isolated with a plan.
