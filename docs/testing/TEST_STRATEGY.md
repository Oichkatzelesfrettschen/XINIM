# Xinim Test Strategy

Version: 0.1
Date: 2025-12-31
Status: Draft (active)

## Goals
- Validate kernel, servers, and userland behavior on every change.
- Keep QEMU boot coverage for critical paths.
- Enable deterministic, CI-friendly test runs.

## Test Tiers
1. Unit tests: fast, isolated component tests.
2. Integration tests: cross-module verification (IPC, VFS, MM, scheduler).
3. POSIX compliance: targeted compliance suites.
4. Boot validation: QEMU boot and smoke tests.
5. Performance baselines: benchmark critical algorithms.
## CTest Integration
- All tests are registered in CMake with labels: unit, integration, posix, qemu.
- `ctest -L unit` runs unit tests only.
- `ctest -L qemu` runs QEMU boot tests and checks log output.

## Boot Validation
- QEMU boot scripts produce log artifacts under logs/.
- A smoke test asserts expected boot milestones in the serial log.

## Regression Policy
- Warnings are treated as errors in test builds.
- New features require tests or explicit waivers documented in PR notes.
