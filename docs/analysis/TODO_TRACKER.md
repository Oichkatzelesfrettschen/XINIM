# TODO/FIXME Triage Tracker

Date: 2026-03-08
Status: Active

This tracker is for live work only. Historical wrapper-script and external
sysroot references have been removed from the active plan and should be treated
as archival context, not current engineering work.

Primary sequencing reference:
- [ROADMAP_100_STEP.md](/home/eirikr/Github/XINIM/docs/analysis/ROADMAP_100_STEP.md)

## Active Build and Boot Work

- Expand guest validation across the full 32-bit lane matrix:
  - `i586`
  - `i686`
  - `x86_32_core2`
  - `x86_32_athlon`
  - `x86_32_phenom`
- Keep the repo-local GRUB and Limine bootstrap paths deterministic and
  documented.
- Continue burning down warning debt so `-Wconversion` and
  `-Wsign-conversion` can return to full `-Werror`.

## x86_64 Guest Work

- Keep the Limine ISO path healthy and covered by real guest tests.
- Continue shell reachability and boot-log validation on the image-based flow.

## 32-bit Userland Work

- Grow the bootstrap user ABI beyond the current shell baseline.
- Add more POSIX.1-2008 behavior coverage to `xash`.
- Keep the low-end 486-safe baseline intact while adding later CPU lanes.

## Native Tools Work

- Keep rebuilding hosted tools in native C++.
- Build out `xinim::tools::core` so shell and command work shares one support
  layer.
- Use Heirloom and related shells as behavior references only.
- Keep promoting only commands that survive warnings-as-errors cleanly and have
  at least smoke-level validation.
- keep this command inventory synchronized with `xinim_commands_hosted` and
  `src/commands/tests` after each wiring batch
- keep `docs/analysis/HOSTED_COMMAND_INVENTORY.md` list aligned with each
  command retirement wave and explicit archive moves

## Immediate Tranche

- keep shrinking `src/commands` with small standalone tools
- keep modernizing remaining fs regressions off the old `filesystem_ops` path
- keep correcting overstated POSIX/QEMU/build claims in the active docs
- keep burning down total unwired files across `src`, `test`, and `userland`
- keep `userland` at zero unwired files now that the hosted sample and mksh
  reference lane are in the graph
- prioritize low-dependency promotions or clear quarantine decisions over
  leaving stale half-wired files in place
- keep the deferred command-candidate queue explicit (currently zero pending in
  `src/commands`, with retired items tracked under
  `archive/legacy/commands/`)
- keep the archival command queue explicit in `archive/legacy/commands/` with
  a reversible path back to active review if a command is later promoted
- keep the build graph audit current after each wiring batch
- keep the POSIX-language verifier passing after each doc update
- keep historical placeholder evidence under `archive/legacy/` instead of the
  active source/test tree
- keep relabeling lingering active-doc compliance language so it matches the
  archived-placeholder split
- keep classifying kernel/mm parallel stories like `fd_table` versus VFS FD
  tables while keeping the reconciled DMA compatibility layer aligned with the
  active allocator backend
- keep the 486 DMA lane honest by preserving the below-16MiB plus single-64KiB
  ISA DMA window rule in both freestanding and hosted validation paths
- completed the next 25-file kernel/mm triage wave:
  - retired 13 MM legacy `.cpp` units and one kernel legacy floppy path to
    `archive/legacy/mm/retired` and `archive/legacy/kernel/retired`
  - kept active MM lane focused on `alloc/dma_allocator/dma/pmm/memory/meminfo`
  - re-scanned `src/mm/*`/`src/kernel/*` against the active CMake flow
- continue mining old MM-server files for semantics, but classify remaining
  candidates (`src/mm/signal.cpp`, `src/mm/putc.cpp`, `src/mm/syscall.cpp`,
  `src/kernel/wormhole.cpp`) explicitly instead of leaving them in a gray area
- keep the next 25-file kernel/mm triage wave focused, with active watchlist
  updated in `docs/analysis/BUILD_GRAPH_RECONCILIATION.md`

## Future Architecture Track

- Evaluate future serialization choices through
  [ADR 0010](/home/eirikr/Github/XINIM/docs/adr/0010-serialization-track-msgpack-vs-capnp.md)
  instead of making ad hoc dependency decisions.

## Historical Notes

- Generated analysis snapshots under `docs/analysis/tooling/` and
  `docs/analysis/ast_dependency_*.json` may still contain legacy paths because
  they are recorded artifacts, not active guidance.
