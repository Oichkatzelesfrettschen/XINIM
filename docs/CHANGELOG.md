# XINIM Changelog

## v1.3.0 -- Bare-Metal VFS Implementation (2026-03-05)

Implements ADR-0009: a freestanding ramfs with first functional POSIX syscalls.
Zero STL in the VFS critical path. Fixed ~1.35 MB memory footprint.

### New VFS Subsystem (src/vfs/)

All files are freestanding: -ffreestanding -fno-exceptions -fno-rtti compatible.
No heap allocation in any hot path. All storage is pre-allocated static arrays.

- `bare_vfs.hpp`: Core structs -- RawInode (64B alignas(64)), DirEntry (32B),
  MountEntry (64B), FdEntry (16B), FsOps function pointer table, KStat, CacheBlock.
  Constants: MAX_INODES=1024, MAX_DIRENTS=8192, MAX_MOUNTS=8, MAX_FDS=64,
  CACHE_BLOCKS=64, DATA_ARENA_SIZE=1MB.

- `inode_table.cpp`: Flat inode array + 16-word bitmap allocator.
  O(1) alloc via __builtin_ctzll(~bitmap_word). 1MB data arena for file bodies.
  inode_alloc / inode_free / inode_get / data_arena_alloc / data_arena_ptr.

- `dirent.cpp`: Directory entry arena with DIRENT_BLOCK_SIZE=32 slots per dir.
  dirent_add / dirent_lookup / dirent_remove / dirent_readdir.
  Lookup is O(DIRENT_BLOCK_SIZE) = O(32) per directory level.

- `path_walk.cpp`: Zero-allocation path component slicing.
  Handles "." (skip), ".." (parent_ino lookup), trailing slashes, root.
  path_walk / path_walk_parent (two-pass component collector).

- `ramfs_ops.cpp`: All 8 FsOps implemented for ramfs:
  open / read / write / close / stat / mkdir / unlink / readdir.
  Files <= 24 bytes use inode inline_data (INODE_IS_INLINE flag);
  larger files promoted to data_arena on write. Deferred unlink on close.

- `mount_table.cpp`: Fixed array of 8 mount entries.
  Longest-prefix matching in O(8). Pre-mounts "/" at ino=1.
  mount_add / mount_remove / mount_resolve.

- `fd_table.cpp`: Global FD table with MAX_FDS=64 entries.
  O(MAX_FDS) scan for free slot. fd_allocate / fd_get / fd_release.

- `buffer_cache.cpp`: 64 x 512-byte cache slots. LRU eviction via lru_seq counter.
  O(CACHE_BLOCKS) scan on miss. cache_get / cache_mark_dirty / cache_flush.

- `vfs_server.cpp`: Full VFS IPC message loop replacing the stub.
  Handles: VFS_OPEN, VFS_READ, VFS_WRITE, VFS_CLOSE, VFS_STAT, VFS_MKDIR,
  VFS_UNLINK, VFS_READDIR. Default reply: -ENOSYS.
  vfs_server_init() creates root inode, /bin, /dev, /proc, /tmp.

- `bare_metal_stubs.cpp`: vfs_server_main() now calls vfs_server_init() +
  vfs_server_loop() instead of the ENOSYS stub. Also adds memcmp() to klib64.

### New Tests

- `test/test_bare_vfs.cpp`: 13 host-side unit tests.
  Covers: inode alloc/free, bounds check, mkdir, inline/arena write,
  read roundtrip (small + large), stat, path_walk, unlink, FD lifecycle,
  mount_resolve.

### Test Count Delta

| Release | Host Tests | Passing |
|---------|-----------|---------|
| v1.2.0  | 31        | 31/31   |
| v1.3.0  | 32        | 32/32   |

### POSIX Compliance Progress

- open/read/write/close on ramfs files: FUNCTIONAL (first POSIX progress)
- stat on ramfs files: FUNCTIONAL
- mkdir/unlink: FUNCTIONAL
- SYS_write to fd=1/2 (stdout/stderr): echoes to serial (preserved)
- POSIX compliance rises from 0% toward partial (ramfs round-trips pass)

### Memory Budget (fixed, known at compile time)

| Structure | Size |
|-----------|------|
| Inode table (1024 * ~80B) | ~80 KB |
| Dirent arena (8192 * 32B) | 256 KB |
| Data arena | 1 MB |
| Buffer cache (64 * ~528B) | ~33 KB |
| Mount table (8 * 64B) | 512 B |
| FD table (64 * 16B) | 1 KB |
| **Total** | **~1.37 MB** |

### Known Limitations (carried to v1.4.0)

- POSIX compliance: partial (ramfs functional, no userland process yet)
- FD table: global (per-process tables are v1.4.0)
- Filenames > 26 chars: -ENAMETOOLONG (overflow table deferred)
- Kyber KEM: kem.cpp still missing
- Ring 3: all servers run Ring 0
- virtio-net: skeleton only
- data_arena: no free list (allocations are permanent for v1.3.0)

---

## v1.2.0 -- Foundation Correctness (2026-03-05)

Resolves 75 tasks across 5 implementation phases + integration/docs.
Fixes the kernel's foundational correctness deficits: working memory
reclamation, a single authoritative scheduler, correct IPC semantics,
a real process lifecycle, FPU state preservation, and deadlock-safe locks.

### Phase 1: Kernel Heap Allocator

Replace 1MB bump allocator (no-op free) with 4MB free-list allocator.

- `src/kernel/heap.hpp` / `heap.cpp`: First-fit with forward+backward coalescing,
  16-byte alignment, 32-byte block header. heap_init/heap_alloc/heap_free/heap_stats.
- `src/kernel/klib64.cpp`: malloc() -> heap_alloc(), free() -> heap_free().
  Removed 1MB static bump array; 4MB free-list heap.
- `src/kernel/server_spawn.cpp`: removed 16MB duplicate kmalloc; routes through malloc.
- New test: `test/test_heap_allocator.cpp` (11 tests: alloc/free, coalescing,
  exhaustion, alignment, double-free, stats, fragmentation stress).

### Phase 2: Unified O(1) Scheduler

Replaced two incompatible schedulers (proc.cpp priority queue + schedule.cpp flat FIFO)
with a single `UnifiedScheduler`.

- `src/kernel/unified_scheduler.hpp` / `unified_scheduler.cpp`:
  O(1) bitmap scheduler: uint64_t priority_bitmap + per-priority doubly-linked PCB queues.
  pick_next() uses `__builtin_ctzll()` -- 1 instruction to find highest priority.
  64 priority levels; configurable per-priority quanta; WaitForGraph deadlock detection.
- `src/kernel/scoped_irq_lock.hpp`: RAII interrupt disable/restore (pushfq/cli/popfq).
- `src/kernel/sys/dispatch.cpp`: sys_getpid_impl returns real current PID.
  Caller PID from g_unified_scheduler.current_pid() (was hardcoded 1).
- `src/kernel/timer.cpp`: timer_interrupt_handler_c calls g_unified_scheduler.timer_tick().
- New test: `test/test_unified_scheduler.cpp` (13 tests: empty, priority ordering,
  FIFO, block/unblock, deadlock detection, yield, quantum, bitmap consistency).

### Phase 3: IPC Hardening

Fixed silent data corruption on channel exhaustion, added back-pressure.

- Channel QUEUE_SIZE: 8 -> 32. Channel::connect() returns nullptr (E_CHAN_FULL) on full.
- Added `E_CHAN_FULL`, `E_QUEUE_FULL`, `E_NO_CHANNEL`, `E_DEADLOCK` to sys/error.hpp.
- Blocking send: blocks sender when queue full; receiver unblocks on pop.
- Blocking receive: blocks receiver when no message; sender unblocks on push.
- Message source tagging: msg.m_source set in lattice_send.
- Per-process incoming bitset (incoming_channels_[]) for O(1) recv.
- New tests: `test_ipc_blocking.cpp` (5 tests), `test_ipc_exhaustion.cpp` (2 tests).

### Phase 4: Process Lifecycle and FPU State

Implemented correct process exit/wait and FPU state preservation.

- `src/kernel/process_lifecycle.hpp` / `process_lifecycle.cpp`:
  process_exit(): ZOMBIE state, stack cleanup via heap_free, parent notification.
  process_wait(): zombie child scan, reap (DEAD), block parent on none.
- `src/kernel/sys/dispatch.cpp`: sys_exit_impl calls process_exit (was: infinite halt).
  SYS_wait4 handled in kernel via process_wait (was: routed to PM stub).
- `src/kernel/context.hpp`: added alignas(16) uint8_t fxsave_area[512].
  CpuContext: 208 -> 720 bytes. initialize() sets MXCSR=0x1F80.
- `src/arch/x86_64/context_switch.S`: fxsave/fxrstor at offset 0xD0.
- New test: `test/test_process_lifecycle.cpp` (6 tests).

### Phase 5: Lock Safety and Synchronization

Added MAX_SPINS timeouts to all spin-wait loops; fixed data race; added IRQ guard.

- MCSSpinlock: MCS_MAX_SPINS=100k on lock/unlock successor-wait.
- PhaseRWLock: RWLOCK_MAX_SPINS=100k on read_lock/write_lock.
- TicketSpinlock: TICKET_MAX_SPINS=10M.
- QuaternionSpinlock: removed non-atomic orientation field (data race);
  simplified to pure TAS spinlock; ticket parameter is cosmetic.
- MCSIrqLockGuard: RAII that disables interrupts before MCS lock acquisition.
- at_wini.cpp, xt_wini.cpp: ScopedPortLock::~ScopedPortLock uses restore()
  instead of unlock() (preserves RFLAGS from lock()).
- New test: `test/test_lock_timeout.cpp` (10 tests).

### Phase 6: Integration, Documentation, and Hardening

- cmake/CompilerWarnings.cmake: promoted -Wshadow to -Werror=shadow (0 violations).
  -Wconversion/-Wsign-conversion remain warnings pending v1.3.0 cleanup.
- docs/analysis/TODO_TRACKER.md: 3 PHASE6 items marked REMOVED (resolved in v1.2.0).
- docs/analysis/CLAIMS_AUDIT.md: updated for v1.2.0 test count (31) and subsystems.
- 31/31 host-side unit tests pass.

### Test Count Delta

| Release | Host Tests | Passing |
|---------|-----------|---------|
| v1.0.0  | 20        | n/a     |
| v1.1.0  | 25        | 25/25   |
| v1.2.0  | 31        | 31/31   |

### Known Limitations (carried to v1.3.0)

- POSIX compliance: 0% (no userland process; VFS returns ENOSYS)
- Kyber KEM: foundations correct, kem.cpp missing (Phase 9)
- Ring 3: all servers run Ring 0 (Phase 9)
- virtio-net: skeleton only (Phase 10)
- -Wconversion/-Wsign-conversion: 13 files with warnings, blocked by v1.1.0 code
- QEMU boot: infrastructure wired, validation requires CI QEMU runner

---

## v1.1.0 -- Technical Debt Resolution (2026-02-26)

This release resolves 150 atomic tasks across 8 phases, addressing
build-system debt, documentation accuracy, test coverage, code safety,
architectural alignment, and feature completeness.

### Summary of Changes

| Phase | Focus | Key Outcomes |
|-------|-------|--------------|
| 1 | Repo hygiene | Removed xmake remnants, stale build artifacts, tracked deleted files |
| 2 | Build hardening | Per-target includes, -ffreestanding on kernel, Doxygen target, -Werror |
| 3 | Documentation | < 25 active docs, false README claims corrected, ADRs 0003-0005 |
| 4 | Test expansion | 25 registered CTest tests (was 20), labeled by category |
| 5 | Code quality | TODO tracker (125 items triaged), snprintf/strncpy replacements |
| 6 | Architecture | Microkernel boundary doc, VFS/PM/MM server message loops, IPC tests |
| 7 | Features | 25 dispatched syscalls, ELF parser test, virtio-net skeleton |
| 8 | Integration | clang-tidy clean on new files, cppcheck, Doxygen, 25/25 tests pass |

### Bug Fixes (prior sessions, documented here for completeness)
- Fixed null-dereference in schedule.cpp (was: cur_proc deref with no guard)
- Fixed iretq in interrupts.S (was: missing cs push in isr wrapper)
- Fixed noreturn attribute missing on panic() (undefined behavior at callsite)
- Fixed AVX2 subtraction in kyber_cpp23_simd.cpp (_mm256_add -> _mm256_sub)
- Fixed Keccak piln permutation indices in fips202.cpp (NIST KAT now passes)
- Fixed Octonion sub-product sign error (Cayley-Dickson rule)
- Unified IDT path (idt64.cpp superseded by interrupts.cpp)
- Dual serial: COM1=logs, COM2=kshell (TCP:4555)

### Test Infrastructure (Phase 4-7)
- test_kyber_e2e: Kyber512 constant validation (E2E pending kem sources)
- test_service_manager_api: ServiceManager current API
- test_net_driver_stub: NetDriver stub returns failure values
- test_syscall_dispatch: Syscall table constants and ranges
- test_ipc_channel: Lattice IPC Channel FIFO, overflow, wrap-around
- test_elf_parser: ELF64 header validation, flags-to-prot conversion
- test/stubs/serial_stub.cpp: No-op Serial16550 + vfs stubs for host tests

### Architecture (Phase 6-7)
- docs/specs/MICROKERNEL_BOUNDARY.md: kernel vs server responsibilities
- docs/specs/IPC_PROTOCOL.md: message struct, Channel model, server endpoints
- docs/specs/BOOT_SEQUENCE.md: boot path from Limine to kshell
- docs/adr/0006-vfs-implementation-path.md: vfs_server over legacy src/fs/
- docs/adr/0007-network-driver-architecture.md: virtio-net target
- docs/adr/0008-post-quantum-crypto-integration.md: Kyber512 + ChaCha20-Poly1305
- src/kernel/bare_metal_stubs.cpp: VFS/PM/MM minimal Ring-0 message loops
- src/drivers/net/virtio_net.cpp: virtio-net skeleton (Phase 8: PCI walk)

### Syscall Dispatch Expansion (Phase 7)
Dispatch table expanded from 12 to 25 entries:
- Added: getuid, geteuid, getgid, getegid (return 0)
- Added: brk (bump-allocator stub), mmap (ENOSYS), munmap (no-op)
- Added VFS routes: lseek, dup, dup2, pipe, stat, fstat

### Known Limitations (carried forward to v1.2.0)
- POSIX compliance: 0% (25 dispatched, 4 truly functional)
- Kyber keygen/encap/decap: primitives correct, kem.cpp missing
- VFS server handlers: stub ENOSYS (Phase 8: ramfs open/read/write/close)
- MCS spinlock and PhaseRWLock: cooperative design, not preemption-safe
- Ring 3 transition: all servers run Ring 0 (Phase 9)
- virtio-net: skeleton only, PCI walk not yet implemented

### Baseline (v1.0.0 -- before debt resolution)
- 20 unit tests registered
- 9 critical bugs present
- xmake build system remnants
- 90+ documentation files, no canonical index
- 12 dispatched syscalls
