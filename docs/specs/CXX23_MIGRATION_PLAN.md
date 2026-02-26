# C++23 Migration Plan

Version: 0.1
Date: 2025-12-31
Status: Draft (active)

## Goal
Eliminate all C translation units and standardize on ISO C++23 across the
entire repository.

## Phase 1: Build System Prep
- Ensure CMake treats all targets as C++23.
- Introduce Conan for third_party dependencies.
- Add tooling to flag new C sources in CI.

## Current C Sources Snapshot
- third_party: ~2095 .c files
- libc: ~1035 .c files
- userland: 4 .c files

## Phase 2: Core Targets
- Convert kernel sources to .cpp where needed.
- Replace C headers with C++ headers and namespaces.
- Add Doxygen coverage for all public APIs.

## Phase 3: Userland + Tests
- Migrate userland command sources to C++23.
- Update tests to use modern C++ fixtures and assertions.

## Phase 4: libc and third_party
- Replace or modernize vendored C libraries.
- Introduce compatibility shims where ABI requires C linkage.
- Track each third_party component with a modernization status record.

## Risk Controls
- Convert and validate in small batches.
- Maintain bootable images after each phase.
- Keep regression tests aligned with each migration step.
