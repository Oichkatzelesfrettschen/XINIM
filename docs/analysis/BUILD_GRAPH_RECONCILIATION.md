# Build Graph Reconciliation

Date: 2026-03-08

## Goal

This note reconciles the biggest unwired source clusters against the active
Conan + CMake graph so the project carries one primary implementation story
per subsystem instead of several implicit ones.

## Filesystem Verdict

Primary active lane:

- compact freestanding VFS under `src/vfs/bare_vfs.hpp`,
  `src/vfs/inode_table.cpp`, `src/vfs/vnode_table.cpp`,
  `src/vfs/dirent.cpp`, `src/vfs/path_walk.cpp`,
  `src/vfs/ramfs_ops.cpp`, `src/vfs/core_init.cpp`,
  `src/vfs/seed.cpp`, `src/vfs/bootfs_promote.cpp`,
  `src/vfs/mount_table.cpp`, `src/vfs/fd_table.cpp`,
  and `src/vfs/buffer_cache.cpp`
- shared `bootfs` promotion path for both x86_32 and x86_64

Reference or legacy lanes:

- `src/fs/*`
  - This is a Minix-style standalone FS server lineage, not the active compact
    VFS used by the current kernel lanes.
  - Keep as reference code for server-style semantics and historical behavior.
  - Do not wire it into the modern kernel/image graph.
- `src/vfs/vfs.cpp`, `src/vfs/vfs_enhanced.cpp`, `src/vfs/vfs_security.cpp`,
  `src/vfs/filesystem.cpp`, `src/vfs/fs_init.cpp`, `src/vfs/mount.cpp`,
  `src/vfs/tmpfs.cpp`, `src/vfs/ext2.cpp`
  - This is a second, STL-heavy hosted or block-backed VFS design.
  - It does not match the low-RAM freestanding compact VFS constraints.
  - Keep as a prototype/reference track for future hosted or larger-memory
    block-backed filesystems, but not as the active kernel VFS story.

Integration extracted from the legacy/prototype side:

- the high-level `xinim::fs` API is now backed by a hosted compatibility layer
  in `src/host/filesystem_host_compat.cpp`
- this lets curated hosted commands reuse one filesystem abstraction instead of
  each command bypassing the project API surface

## Commands Verdict

Keep and wire as hosted tools now:

- `src/commands/echo.cpp`
- `src/commands/pwd.cpp`
- `src/commands/true_cpp23.cpp`
- `src/commands/false_cpp23.cpp`
- `src/commands/mkdir.cpp`
- `src/commands/rm.cpp`
- `src/commands/touch.cpp`
- `src/commands/cp.cpp`
- `src/commands/ls.cpp`
- `src/commands/cat_cpp23.cpp`
- `src/commands/wc.cpp`
- `src/commands/ln.cpp`
- `src/commands/chmod.cpp`
- `src/commands/basename.cpp`
- `src/commands/head.cpp`
- `src/commands/env_cpp23.cpp`
- `src/commands/sleep.cpp`
- `src/commands/rev.cpp`
- `src/commands/tee.cpp`
- `src/commands/cut_cpp23.cpp`
- `src/commands/rmdir.cpp`
- `src/commands/sum.cpp`
- `src/commands/tr.cpp`
- `src/commands/date.cpp`
- `src/commands/comm.cpp`
- `src/commands/sync.cpp`
- `src/commands/cmp.cpp`
- `src/commands/uniq.cpp`
- `src/commands/od.cpp`
- `src/commands/kill.cpp`
- `src/commands/chown.cpp`
- `src/commands/mv.cpp`
- `src/commands/df.cpp`

Keep but defer:

- None pending.

Canonicalize or retire:

- prefer `cat_cpp23.cpp` over `cat.cpp`
- prefer `echo.cpp` over `echo_cpp23.cpp`
- keep the `mined*` editor family out of the default graph for now; it is
  substantial but not yet cohesive enough for the active toolchain lane
- moved and archived legacy command candidates:
  - `archive/legacy/commands/awk_cpp23.cpp`
  - `archive/legacy/commands/async_grep_cpp23.cpp`
  - `archive/legacy/commands/cal.cpp`
  - `archive/legacy/commands/cc.cpp`
  - `archive/legacy/commands/chmem.cpp`
  - `archive/legacy/commands/size.cpp`
  - `archive/legacy/commands/split.cpp`
  - `archive/legacy/commands/ps_cpp23.cpp`
  - `archive/legacy/commands/pwd_cpp23.cpp`
  - `archive/legacy/commands/cat.cpp`
  - `archive/legacy/commands/ar.cpp`
  - `archive/legacy/commands/clr.cpp`
  - `archive/legacy/commands/dd.cpp`
  - `archive/legacy/commands/dosread.cpp`
  - `archive/legacy/commands/encrypt_cpp23.cpp`
  - `archive/legacy/commands/grep.cpp`
  - `archive/legacy/commands/getlf.cpp`
  - `archive/legacy/commands/lpr.cpp`
  - `archive/legacy/commands/make.cpp`
  - `archive/legacy/commands/mknod.cpp`
  - `archive/legacy/commands/mined.cpp`
  - `archive/legacy/commands/mined_editor.cpp`
  - `archive/legacy/commands/mined_editor_complex.cpp`
  - `archive/legacy/commands/mined_final.cpp`
  - `archive/legacy/commands/mined_library.cpp`
  - `archive/legacy/commands/mined_main.cpp`
  - `archive/legacy/commands/mined_main_complex.cpp`
  - `archive/legacy/commands/mined_simple.cpp`
  - `archive/legacy/commands/mined_unified.cpp`
  - `archive/legacy/commands/roff.cpp`
  - `archive/legacy/commands/constexpr_date_cpp23.cpp`
  - `archive/legacy/commands/echo_cpp23.cpp`
  - `archive/legacy/commands/gres.cpp`
  - `archive/legacy/commands/libpack.cpp`
  - `archive/legacy/commands/libupack.cpp`
  - `archive/legacy/commands/login.cpp`
  - `archive/legacy/commands/mkfs.cpp`
  - `archive/legacy/commands/mount.cpp`
  - `archive/legacy/commands/passwd.cpp`
  - `archive/legacy/commands/pr.cpp`
  - `archive/legacy/commands/pr_modern.cpp`
  - `archive/legacy/commands/shar.cpp`
  - `archive/legacy/commands/simd_wc_cpp23.cpp`
  - `archive/legacy/commands/sort.cpp`
  - `archive/legacy/commands/sort_modern.cpp`
  - `archive/legacy/commands/stty.cpp`
  - `archive/legacy/commands/sh1.cpp`
  - `archive/legacy/commands/sh3.cpp`
  - `archive/legacy/commands/sh4.cpp`
  - `archive/legacy/commands/sh5.cpp`
  - `archive/legacy/commands/su.cpp`
  - `archive/legacy/commands/svcctl.cpp`
  - `archive/legacy/commands/tail.cpp`
  - `archive/legacy/commands/test_mined_console.cpp`
  - `archive/legacy/commands/tar.cpp`
  - `archive/legacy/commands/tar_modern.cpp`
  - `archive/legacy/commands/time.cpp`
  - `archive/legacy/commands/umount.cpp`
  - `archive/legacy/commands/update.cpp`
  - `archive/legacy/commands/x.cpp`

## Kernel And MM Verdict

Active or worth preserving in the modern graph now:

- `src/kernel/exec_stack.cpp`
  - useful pure helper logic for `execve` stack layout
  - now host-tested
- `src/kernel/uaccess.cpp`
  - compact and self-contained user pointer validation/copy surface
  - now host-tested
- `src/mm/pmm.cpp`
  - modern PMM implementation with zones and buddy allocation
  - now host-tested
- `src/mm/meminfo.cpp`
  - useful read-only instrumentation layer over the modern PMM
  - now host-tested
- `src/mm/dma_allocator.cpp`
  - active helper for DMA-capable allocation paths and stats queries
  - now host-tested at the non-driver utility level
- `src/kernel/pipe.cpp`
  - compact kernel IPC primitive with a clear behavioral surface
  - now host-tested with scheduler/signal stubs instead of remaining stranded

Scoped but deferred:

- `src/kernel/process_group.cpp`
  - still interesting and likely active, but it is coupled to real PCB,
    scheduler, signal, and early-serial state, so it needs a more intentional
    host harness instead of a shallow compile-only promotion
- `src/mm/dma.cpp`
  - this is a second DMA API story with a different object model than the
    already-promoted `dma_allocator.cpp` path
  - mine semantics later, but do not mix both DMA stories into the active graph
    without first choosing a canonical API

Lane-specific and still active:

- `src/kernel/i486/*`
- `src/kernel/x86_64/*`
- selected modern scheduler, process, interrupt, and boot modules already wired
  into the current image targets
- `src/mm/alloc.cpp`
- `src/mm/pmm.cpp`
- `src/mm/dma.cpp`
- `src/mm/dma_allocator.cpp`
- `src/mm/memory.cpp`
- `src/mm/meminfo.cpp`

Reference or likely retired for the default graph:

- `archive/legacy/kernel/lane_specific/minix/*`
  - historical/reference implementation lane preserved for archaeology
  - includes `console.cpp`, `kernel.cpp`, `pmm.cpp`, `vmm.cpp`, `stub.cpp`
- `archive/legacy/kernel/retired/acpi_stub.cpp`
  - ACPI stub compatibility placeholder from early wiring experiments
- `archive/legacy/kernel/retired/idt64.cpp`
  - legacy IDT implementation superseded by `src/kernel/interrupts.cpp`
- `archive/legacy/kernel/retired/ipc_test.cpp`
  - standalone kernel IPC test harness kept for legacy reference
- `archive/legacy/kernel/retired/mpx64.cpp`
  - legacy multiprocessing scaffold not used by active lane
- `archive/legacy/kernel/retired/mpx88.cpp`
  - legacy multiprocessing scaffold not used by active lane
- `archive/legacy/kernel/retired/pqcrypto.cpp`
  - isolated crypto helper reference, not active today
- `archive/legacy/kernel/retired/signal.cpp`
  - old signal subsystem for legacy MM-server model
- `archive/legacy/kernel/retired/sys_stub.cpp`
  - no longer wired stub dependency
- `archive/legacy/kernel/retired/syscalls/basic.cpp`
  - legacy syscall split; active logic now in consolidated syscall table path
- `archive/legacy/kernel/retired/syscalls/exec.cpp`
  - legacy syscall split; active logic now in consolidated syscall table path
- `archive/legacy/kernel/retired/syscalls/fd_advanced.cpp`
  - legacy syscall split; active logic now in consolidated syscall table path
- `archive/legacy/kernel/retired/syscalls/file_ops.cpp`
  - legacy syscall split; active logic now in consolidated syscall table path
- `archive/legacy/kernel/retired/syscalls/process_mgmt.cpp`
  - legacy syscall split; active logic now in consolidated syscall table path
- `archive/legacy/kernel/retired/syscalls/signal.cpp`
  - legacy syscall split; active logic now in consolidated syscall table path
- `archive/legacy/kernel/retired/time_stub.cpp`
  - test/stub time implementation retired from active path
- `archive/legacy/kernel/retired/timing.cpp`
  - legacy timing helper for timer experiments
- `archive/legacy/kernel/retired/tty_signals.cpp`
  - terminal signal coupling tied to legacy process/serial state
- `archive/legacy/kernel/retired/vfs_interface.cpp`
  - older VFS interface shim superseded by compact VFS server path
- `archive/legacy/mm/retired/main.cpp`
  - MM main loop and MM init path for legacy server model
- `archive/legacy/mm/retired/table.cpp`
  - legacy MM syscall table wiring
- `archive/legacy/mm/retired/exec.cpp`
  - legacy `do_exec` path from old MM-server model
- `archive/legacy/mm/retired/forkexit.cpp`
  - legacy fork/exit/wait/MM cleanup path
- `archive/legacy/mm/retired/getset.cpp`
  - legacy process identity helper path
- `archive/legacy/mm/retired/break.cpp`
  - legacy `do_brk` and stack growth behavior
- `archive/legacy/mm/retired/signal.cpp`
  - legacy signal routing model
- `archive/legacy/mm/retired/utility.cpp`
  - legacy utility layer for MM internals
- `archive/legacy/mm/retired/vm.cpp`
  - legacy virtual memory area handling
- `archive/legacy/mm/retired/paging.cpp`
  - legacy paging helper layer
- `archive/legacy/mm/retired/process_slot.cpp`
- `archive/legacy/mm/retired/process_slot.hpp`
- `archive/legacy/mm/retired/token.hpp`
- `archive/legacy/kernel/retired/xt_wini.cpp`
  - legacy floppy/Winchester path with no active entry point
- `src/mm/vm.cpp`
  - still an interesting prototype; retained as a reference while DMA and memory
    subsystems are consolidated.

Interesting pieces mined but not promoted yet:

- `src/kernel/process_group.cpp`
  - retained in source for future process-group integration decisions

- `src/kernel/wormhole.cpp`
  - retained for optional networking pathway experiments

Immediate next-wave watchlist (`unmapped` after this 25-file triage pass):

- `src/mm/putc.cpp`
- `src/mm/syscall.cpp`

Recently retired this cycle:

- `archive/legacy/mm/retired/main.cpp`
- `archive/legacy/mm/retired/table.cpp`
- `archive/legacy/mm/retired/signal.cpp`
- `archive/legacy/mm/retired/getset.cpp`
- `archive/legacy/mm/retired/break.cpp`
- `archive/legacy/mm/retired/exec.cpp`
- `archive/legacy/mm/retired/forkexit.cpp`
- `archive/legacy/mm/retired/process_slot.cpp`
- `archive/legacy/mm/retired/process_slot.hpp`
- `archive/legacy/mm/retired/token.hpp`
- `archive/legacy/mm/retired/utility.cpp`
- `archive/legacy/mm/retired/vm.cpp`
- `archive/legacy/mm/retired/paging.cpp`


## Current Outcome

The project now has:

- one explicit primary filesystem story for kernels and images
- one explicit hosted filesystem compatibility layer for curated command tools
- one curated hosted command lane instead of an unwired command graveyard
- a broader curated hosted command lane that now includes link, chmod,
  basename, head, env, sleep, rev, tee, cut, sum, tr, date, comm, sync, cmp,
  uniq, od, kill, chown, mv, and df behavior
- `kill`, `chown`, `mv`, and `df` are now included in the aggregate
  `xinim_commands_hosted` target so they are treated as active for hosted lane
  batch verification
- five previously orphaned modern subsystem candidates (`exec_stack`,
  `uaccess`, `pmm`, `meminfo`, and `dma_allocator`) promoted into host
  verification
- additional hosted filesystem regressions for remove, copy_symlink, and
  rename are now wired into the default host test matrix
- `get_status` now has a modernized host regression test using the active
  `xinim::fs::get_status()` free-function API instead of the old
  `filesystem_ops` object path
- `create_directory` and `create_directories` now both have active host
  regression coverage on the current free-function API
- `copy_file` now has active host regression coverage on the current
  free-function API, and the hosted fs wrapper now treats `skip_existing` as a
  successful no-op while mapping directory sources to `is_a_directory`
- `change_ownership` now has active host regression coverage, and the hosted fs
  wrapper explicitly reports `operation_not_supported` for standard mode while
  keeping direct mode as the active ownership-changing path
- `copy` now has active host regression coverage on the current free-function
  API instead of only a stale `filesystem_ops`-era matrix
- `src/kernel/fd_table.cpp` is no longer stuck in the unwired middle; it now
  has active host regression coverage as a kernel utility rather than only
  references from syscall design docs
- `src/mm/memory.cpp` is no longer unwired; it now has a tiny active host test
  that keeps the current initialization stub visible in the graph
- `src/mm/dma.cpp` is no longer a parallel dead-end; it has been reconciled
  into a compatibility layer on top of the active `dma_allocator` backend and
  now has dedicated host coverage
- `src/kernel/syscall_table.cpp` is no longer stranded in the unwired middle;
  it now has active host regression coverage with explicit dispatch/counting
  checks against the current syscall table
- `src/kernel/pipe.cpp` is no longer stranded in the unwired middle; it now
  has active host regression coverage for ring-buffer, EOF, wakeup, and broken
  pipe behavior
- `src/kernel/*` triage wave complete:
  - 25 additional unmapped kernel `.cpp` files moved to
    `archive/legacy/kernel/{retired,lane_specific}`
- active documentation now distinguishes verified reality from roadmap goals
  more clearly for POSIX and x86_64 QEMU bring-up
- active-doc wording now has an enforceable guard via
  `scripts/verify_posix_language.py` and the `posix_language_check` test/target
- `userland/tests/hello.c` is now wired as a hosted sample, and the three
  `userland/shell/mksh/integration/*.c` files are compiled as an explicit
  reference library so the entire `userland` tree is at `0` unwired files in
  the current audit
- the canonical DMA story is now `dma_allocator` as the backend plus
  `dma.hpp`/`dma.cpp` as compatibility helpers, rather than two unrelated DMA
  implementations
- the canonical DMA lane now enforces the 486/ISA-style low-memory constraint
  more honestly: `BELOW_16MB` means below 16 MiB and within one 64 KiB DMA
  window, and the hosted tests use a synthetic low-memory physical aperture so
  that this behavior is actually testable instead of being hidden behind host
  virtual addresses
- `archive/legacy/commands/awk_cpp23.cpp`,
  `archive/legacy/commands/async_grep_cpp23.cpp`,
  `archive/legacy/commands/cal.cpp`, `archive/legacy/commands/cc.cpp`,
  `archive/legacy/commands/chmem.cpp`, `archive/legacy/commands/size.cpp`,
  `archive/legacy/commands/split.cpp`,
  `archive/legacy/commands/ps_cpp23.cpp`,
  `archive/legacy/commands/pwd_cpp23.cpp`,
  `archive/legacy/commands/cat.cpp`, `archive/legacy/commands/ar.cpp`,
  `archive/legacy/commands/clr.cpp`, `archive/legacy/commands/dd.cpp`,
  `archive/legacy/commands/dosread.cpp`,
  `archive/legacy/commands/encrypt_cpp23.cpp`,
  `archive/legacy/commands/grep.cpp`, `archive/legacy/commands/getlf.cpp`,
  `archive/legacy/commands/lpr.cpp`, `archive/legacy/commands/make.cpp`,
  `archive/legacy/commands/mknod.cpp`,
  `archive/legacy/commands/mined.cpp`,
  `archive/legacy/commands/mined_editor.cpp`,
  `archive/legacy/commands/mined_editor_complex.cpp`,
  `archive/legacy/commands/mined_final.cpp`,
  `archive/legacy/commands/mined_library.cpp`,
  `archive/legacy/commands/mined_main.cpp`,
  `archive/legacy/commands/mined_main_complex.cpp`,
  `archive/legacy/commands/mined_simple.cpp`,
  `archive/legacy/commands/mined_unified.cpp`,
  `archive/legacy/commands/constexpr_date_cpp23.cpp`,
  `archive/legacy/commands/echo_cpp23.cpp`,
  `archive/legacy/commands/gres.cpp`,
  `archive/legacy/commands/libpack.cpp`,
  `archive/legacy/commands/libupack.cpp`,
  `archive/legacy/commands/login.cpp`,
  `archive/legacy/commands/mkfs.cpp`,
  `archive/legacy/commands/mount.cpp`,
  `archive/legacy/commands/passwd.cpp`,
  `archive/legacy/commands/pr.cpp`,
  `archive/legacy/commands/pr_modern.cpp`,
  `archive/legacy/commands/roff.cpp`,
  `archive/legacy/commands/shar.cpp`,
  `archive/legacy/commands/sh1.cpp`,
  `archive/legacy/commands/sh3.cpp`,
  `archive/legacy/commands/sh4.cpp`,
  `archive/legacy/commands/sh5.cpp`,
  `archive/legacy/commands/simd_wc_cpp23.cpp`,
  `archive/legacy/commands/sort.cpp`,
  `archive/legacy/commands/sort_modern.cpp`,
  `archive/legacy/commands/stty.cpp`,
  `archive/legacy/commands/su.cpp`,
  `archive/legacy/commands/svcctl.cpp`,
  `archive/legacy/commands/tail.cpp`,
  `archive/legacy/commands/test_mined_console.cpp`,
  `archive/legacy/commands/tar.cpp`, and
  `archive/legacy/commands/tar_modern.cpp`,
  `archive/legacy/commands/time.cpp`,
  `archive/legacy/commands/umount.cpp`,
  `archive/legacy/commands/update.cpp`, and
  `archive/legacy/commands/x.cpp` were re-checked and moved to
  archived legacy status, so they stay out of the active graph for now.
- the old unwired POSIX compliance placeholder tool/tests have been moved under
  `archive/legacy/posix_placeholders/` so they stop competing with the active
  evidence-backed story

That is still not the final cleanup, but it is a meaningful reduction in
parallel, conflicting implementation stories.
