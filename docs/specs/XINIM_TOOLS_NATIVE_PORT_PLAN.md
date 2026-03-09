# XINIM Tools Native Port Plan

Date: 2026-03-08
Status: Active

## Purpose

Define how XINIM rebuilds shell and userland tools as native C++ programs
inside this repository while using Heirloom and related shells as behavior
references rather than shipped implementation sources.

## Current Repo Anchors

- Canonical build graph:
  [CMakeLists.txt](/home/eirikr/Github/XINIM/CMakeLists.txt)
- Canonical build entrypoints:
  [BUILD.md](/home/eirikr/Github/XINIM/docs/BUILD.md)
- Hosted shell sources:
  [main.cpp](/home/eirikr/Github/XINIM/userland/shell/xinim-sh/main.cpp)
- Guest bootstrap shell sources:
  [main_i486.cpp](/home/eirikr/Github/XINIM/userland/shell/xash/main_i486.cpp)
- Heirloom reference tree:
  `/home/eirikr/Github/heirloom-project-custom`

## Non-Negotiable Rules

- Shipped `xinim-tools` binaries must be native C++.
- The ground-truth build lives in the repo's Conan + CMake flow.
- Conan may support hosted tools and libraries, but never the freestanding
  kernel runtime.
- Heirloom, ash, mksh, and bash are reference corpora for behavior, not the
  final implementation payload.

## Shell Contract

Direction:
- canonical shell identity: `xash`
- first target: reliable hosted and guest bootstrap behavior
- next target: POSIX.1-2008 shell semantics on top of that baseline

Current minimum guest contract:
- prompt comes up over the serial shell path
- `help`
- `exit`
- `echo`
- `pid`
- simple file and path checks needed by the current shell tests

## Planned Structure

1. `xinim::tools::core`
   - shared process helpers
   - path/environment helpers
   - terminal and error/reporting primitives
2. `xinim::tools::shell`
   - parser
   - builtins
   - job control
   - execution model
3. `xinim::tools::commands`
   - shared utility code
   - per-command entry points

## Intake Strategy

Stage 1:
- inventory shell behavior and edge cases from the reference shells
- document the behavior worth preserving on low-memory x86 systems

Stage 2:
- keep rebuilding shell behavior inside `xash`
- prefer simple ownership, bounded allocation, and explicit execution paths

Stage 3:
- rebuild small tools first:
  - `echo`
  - `pwd`
  - `test`
  - `getopt`

Stage 4:
- retarget the hosted tool layer into fuller XINIM-native userland as the
  32-bit ABI grows

## Conan Policy

Allowed scope:
- hosted tool dependencies
- developer tooling
- benchmarking and validation

Candidate packages to evaluate only when directly needed:
- `fmt`
- `cli11`
- `tomlplusplus`
- `benchmark`
- `libarchive`

Do not add a package until a native tool target actually uses it.
