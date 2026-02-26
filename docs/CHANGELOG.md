# XINIM Changelog

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
