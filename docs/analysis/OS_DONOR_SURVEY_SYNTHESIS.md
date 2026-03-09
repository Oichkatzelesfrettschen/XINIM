# Cross-Project OS Donor Survey and Synthesis

Date: 2026-03-08
Repo: XINIM
Method: `rg` inventory under `/home/eirikr` and focused source/doc inspection under `/home/eirikr/Github`.

## Scope

Requested themes:
- Solaris / OpenSolaris / illumos
- BSD
- Mach
- Integration path into XINIM for a shared 32-bit / 64-bit structure
- dietlibc baseline plus a practical "dietlibc++" path

## High-Signal Donor Inputs

1. `heirloom-project-custom`
- Strong OpenSolaris userland lineage (not kernel code), especially shell/tooling.
- Notable compatibility signal: `heirloom-sh` documents builds on `diet libc`, Solaris, and multiple BSDs.
- Use in XINIM: userland command semantics, parser behavior, and shell compatibility patterns.

2. `feuerbird_exokernel`
- Mixed maturity, but has concrete design artifacts for multi-profile syscall architecture.
- Contains compatibility-oriented headers and IPC sketches:
  - `include/svr4_machdep.h` (NetBSD-origin SVR4 compatibility definitions)
  - `archive/experimental/ipc/unified_ipc.c` (unified IPC concept layer)
- Use in XINIM: decomposition ideas for ABI compatibility translation and IPC boundary design.

3. `gnu-hurd-docker`
- Primarily documentation, but rich Mach comparative analysis.
- Use in XINIM: architectural lessons about pure microkernel vs hybrid fast path tradeoffs.

## What Not to Treat as Donor Code

- VM snapshots, disk images, logs, and ISO artifacts from `~/VMs` and `~/Downloads`.
- Repos with keyword noise from generic "machine" naming but no direct Solaris/BSD/Mach design leverage.

## XINIM Current-State Fit

Current repo already has strong prerequisites for a dual-lane shared architecture:
- Lane-aware build graph and toolchain configuration in `CMakeLists.txt`.
- Explicit `x86_64` and `i486` entry paths.
- Shared subsystems (`src/vfs`, significant parts of `src/mm`, common include tree).
- dietlibc baseline present under `libc/dietlibc-xinim`.

Gap to close:
- A formal syscall/compatibility translation boundary for Linux/BSD/SVR4/Mach-flavored overlays.

## Synthesized Architecture Direction

1. Keep kernel core lean and shared:
- Scheduler, memory primitives, VFS core, fd table, pipe primitives.

2. Separate architecture glue:
- `arch/x86_64` and `kernel/i486` remain arch-specific for traps, entry, and low-level ABI calling details.

3. Add a lattice changer translation edge:
- `native` path: direct XINIM syscall ABI.
- `linux`, `bsd`, `svr4_illumos`: mapping tables + compatibility shims.
- `mach`: IPC/message bridge, not raw syscall mapping.

4. Keep userland compatibility in userspace where possible:
- Shell/tool behavior and POSIX utility semantics from OpenSolaris/BSD-inspired userland sources.
- Avoid kernel bloat from policy-heavy emulation unless proven performance critical.

## Immediate Code Changes Landed in This Turn

- Added `include/xinim/abi/lattice_changer.hpp`
- Added `src/kernel/abi/lattice_changer.cpp`

These files establish a starter translation boundary without forcing a wide refactor in one shot.

## Licensing and Provenance Cautions

- OpenSolaris-derived code in Heirloom carries CDDL-origin headers.
- BSD and NetBSD-derived pieces can be integrated with attribution and license preservation.
- Keep donor provenance explicit before any direct source import:
  - source repo/path
  - original license
  - local adaptation rationale

## Execution Slices (Refactor-Ground-Up Build)

1. Wire translator into syscall dispatch behind a feature flag.
2. Move syscall number policy into one canonical mapping table per compatibility profile.
3. Add architecture-neutral ABI tests for 32-bit and 64-bit lanes.
4. Land BSD/SVR4 compatibility wrappers in userland-first order.
5. Add Mach-style message bridge only after core IPC latency baseline is stable.
