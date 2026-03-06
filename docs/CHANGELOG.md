# XINIM Changelog

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
