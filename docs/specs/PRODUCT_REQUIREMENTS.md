# Xinim Product Requirements

Version: 0.1
Date: 2025-12-31
Status: Draft (active)

## Overview
Xinim is a research microkernel OS that is being modernized into a pure C++23
codebase with a unified CMake + Conan build system and reproducible QEMU boot.
This document defines user stories, functional requirements, and acceptance
criteria for the modernization and stabilization effort.

## Goals
- Convert all in-tree source to C++23 (no C translation units remain).
- Standardize build and tooling on CMake + Conan.
- Treat warnings as errors across the entire build.
- Ensure a reproducible QEMU boot path for x86_64.
- Generate API documentation via Doxygen + Sphinx/Breathe.

## Scope
- Full-tree refactor (kernel, servers, userland, tests, tooling, libc, and
  third_party with managed replacement or modernization).
- Build, test, and documentation infrastructure updates.
- Repository layout consolidation (docs/, scripts/, src/, tests/).
## User Stories
- US-001: As a contributor, I can build Xinim with a single CMake preset that
  resolves dependencies via Conan and produces a bootable image.
- US-002: As a developer, I can run a local QEMU boot that reaches a shell
  prompt and captures serial output for debugging.
- US-003: As a maintainer, I can run lint and format checks that enforce C++23
  rules and fail on warnings.
- US-004: As a tester, I can execute unit and integration tests with a single
  CTest invocation and get a summary report.
- US-005: As a documenter, I can generate Doxygen + Sphinx output with one
  command and link API references from docs.
- US-006: As a security reviewer, I can inspect a dependency manifest that
  records each third_party origin and its modernization status.
- US-007: As a CI operator, I can run identical build/test steps on Linux
  runners using CMake presets and Conan profiles.
- US-008: As a systems engineer, I can trace kernel services and IPC behavior
  with structured logs in QEMU.
## Functional Requirements
- FR-001: CMake is the sole build system; xmake and related scripts are removed
  or archived with a migration note.
- FR-002: Conan provides dependency resolution and toolchain integration for
  libsodium, limine, and any other third_party libraries.
- FR-003: All in-tree sources compile as C++23 with CMake standard settings.
- FR-004: Build flags enable warnings-as-errors across supported compilers.
- FR-005: A QEMU x86_64 boot script runs the built image and logs output.
- FR-006: Kernel, server, and userland targets are organized into explicit
  CMake subdirectories with target-based includes/flags.
- FR-007: Documentation build emits Doxygen XML and Sphinx HTML artifacts.
- FR-008: Tests integrate with CTest and can be selected by label.
- FR-009: A dependency manifest lists third_party sources, versions, and
  modernization status.
- FR-010: Repository layout follows docs/, scripts/, src/, tests/, and build/
  conventions with generated artifacts ignored by git.
## Non-Functional Requirements
- NFR-001: Builds are reproducible via documented presets and Conan profiles.
- NFR-002: All code changes include Doxygen comments for public APIs.
- NFR-003: Lint and format checks are deterministic and CI-friendly.
- NFR-004: QEMU boot validation is scripted and repeatable.
- NFR-005: Documentation includes decision logs and modernization progress.

## Acceptance Criteria
- AC-001: `cmake --preset <name>` builds kernel, userland, and tests without
  warnings.
- AC-002: `ctest --output-on-failure` runs unit/integration suites successfully.
- AC-003: `scripts/qemu_x86_64.sh` boots to a shell prompt and logs serial IO.
- AC-004: `doxygen docs/Doxyfile` and `sphinx-build` complete without errors.
- AC-005: `clang-tidy` and `clang-format` run clean on modified sources.
