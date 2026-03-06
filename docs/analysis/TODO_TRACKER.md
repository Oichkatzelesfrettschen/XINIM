# TODO/FIXME Triage Tracker

Phase 5 baseline: 2026-02-26. Total in src/ (non-legacy): 125 items.
v1.2.0 update: 2026-03-05. Phase 6 REMOVED count below reflects resolved items.
v1.4.0 update: 2026-03-06. VFS bare-metal files (src/vfs/bare_*.cpp etc.) have zero TODOs.
Legacy VFS files (src/vfs/vfs.cpp, tmpfs.cpp, etc.) are not in the kernel build target;
their TODOs are catalogued below as DEFERRED.

## Triage Categories

- **PHASE6**: Requires microkernel boundary work (IPC, server activation)
- **PHASE7**: Requires feature implementation (VFS, ELF, signals, drivers)
- **DEFERRED**: Valid future work, not blocking current phases
- **REMOVED**: Completed or obsolete; deleted from source

## Kernel (30 TODOs)

| File | Line | Text | Category |
|------|------|------|----------|
| elf_loader.cpp | 191 | Map segment_buf to phdr->p_vaddr in user address space | PHASE7 |
| elf_loader.cpp | 192 | Implement proper VMA and page table management | PHASE7 |
| uaccess.cpp | 59 | Check if pages are actually mapped in process page table | PHASE7 |
| uaccess.cpp | 87 | Set up exception handler to catch page faults | PHASE7 |
| uaccess.cpp | 112 | Set up exception handler to catch page faults | PHASE7 |
| clock.cpp | 54 | implement alarm list with tick-based expiration | PHASE6 |
| irq.cpp | 100 | Detect and initialize APIC/IOAPIC if available | DEFERRED |
| irq.cpp | 245 | Support multiple handlers via linked list | DEFERRED |
| irq.cpp | 298 | For APIC/IOAPIC, configure IOAPIC redirection entry | DEFERRED |
| irq.cpp | 320 | For APIC/IOAPIC, mask IOAPIC redirection entry | DEFERRED |
| irq.cpp | 393 | For APIC, write to Local APIC EOI register | DEFERRED |
| irq.cpp | 477 | Track unhandled interrupts | DEFERRED |
| irq.cpp | 527 | Implement IRQ dump for debugging | DEFERRED |
| syscalls/file_ops.cpp | 82 | Check permissions based on access mode | PHASE7 |
| syscalls/file_ops.cpp | 241 | Free pipe when both ends are closed | PHASE7 |
| syscalls/file_ops.cpp | 244 | Call VFS close on inode (decrement ref count) | PHASE7 |
| syscalls/exec.cpp | 277 | Free old user memory pages | PHASE7 |
| syscalls/exec.cpp | 278 | Implement proper VMA cleanup and page table updates | PHASE7 |
| syscalls/signal.cpp | 81 | Check permissions | PHASE7 |
| adaptive_mutex.hpp | 310 | Add proper initialization check | DEFERRED |
| sys/dispatch.cpp | 43 | return actual cur_proc from proc.cpp | REMOVED (v1.2.0 Phase 2: g_unified_scheduler.current_pid()) |
| sys/dispatch.cpp | 69 | terminate current process, free PCB, notify parent | REMOVED (v1.2.0 Phase 4: process_exit()) |
| sys/dispatch.cpp | 79 | Caller PID -- derive from cur_proc | REMOVED (v1.2.0 Phase 2: g_unified_scheduler.current_pid()) |
| ipc_test.cpp | 52 | Actually send IPC message when lattice_send is available | PHASE6 |
| fd_table.cpp | 227 | Call VFS close on inode | PHASE7 |
| fd_table.cpp | 272 | Increment inode reference count | PHASE7 |
| signal.cpp | 418 | Implement proper process lookup | PHASE7 |
| exec_stack.cpp | 201 | Map stack_buffer to stack_ptr in user address space | PHASE7 |
| exec_stack.cpp | 202 | Implement proper user stack page allocation | PHASE7 |
| server_spawn.cpp | 216 | Implement full IPC registration in lattice_ipc.cpp | PHASE6 |

## Memory Manager (8 TODOs)

| File | Line | Text | Category |
|------|------|------|----------|
| mm/dma_allocator.cpp | 202 | Use proper MMU translation when available | PHASE7 |
| mm/dma_allocator.cpp | 208 | Use proper MMU translation when available | PHASE7 |
| mm/dma.cpp | 108 | Implement cache operations based on architecture | PHASE7 |

(Remaining 5 in mm/ are driver-adjacent DEFERRED items.)

## Crypto (2 TODOs + 3 resolved in v1.4.0)

| File | Line | Text | Category |
|------|------|------|----------|
| crypto/kyber.cpp | 50 | Implement proper AEAD (ChaCha20-Poly1305 or AES-GCM) | PHASE7 |
| crypto/kyber.cpp | 76 | Implement proper AEAD decryption | PHASE7 |
| crypto/kyber_impl/kem.cpp | - | Kyber768 KEM enc/dec roundtrip | REMOVED (v1.4.0 Phase A: indcpa_keypair_derand + poly_compress fixed, 5/5 tests pass) |
| crypto/aes_hw.cpp | - | AES-128 AES-NI acceleration | REMOVED (v1.4.0 Phase E: sw+hw dispatch, NIST test vectors pass) |
| crypto/sha2_hw.cpp | - | SHA-256 SHA-NI acceleration | REMOVED (v1.4.0 Phase E: sw+hw dispatch, NIST test vectors pass) |

## Drivers (20+ TODOs)

All in src/drivers/ and src/block/ -- DEFERRED pending driver implementation phases.

## Commands / Tools (15 TODOs)

Lower priority; not in kernel build path. DEFERRED.

## New bare-metal VFS source files (src/vfs/bare_*.cpp, inode_table.cpp, etc.)

Zero active TODOs in the following v1.3.0/v1.4.0 files (as of 2026-03-06):
bare_vfs.hpp, inode_table.cpp, dirent.cpp, path_walk.cpp, ramfs_ops.cpp,
mount_table.cpp, fd_table.cpp (vfs/), buffer_cache.cpp, vfs_server.cpp.

Legacy VFS files (src/vfs/vfs.cpp, tmpfs.cpp, filesystem.cpp, mount.cpp,
ramfs.hpp, ext2.cpp, path_util.hpp, vfs_enhanced.cpp, vfs_security.cpp)
contain ~25 TODO items but are NOT in the kernel build target. Catalogued
as DEFERRED pending legacy cleanup or deletion.

## Summary

| Category | Count |
|----------|-------|
| PHASE6   | 3     |
| PHASE7   | 20    |
| DEFERRED | 99 + 25 (legacy VFS files) |
| REMOVED  | 6 (3 v1.2.0 + 3 v1.4.0) |
| **Total** | **125 active + 25 legacy-only** |

## Removal Policy

Only remove a TODO when:
1. The work is actually implemented, OR
2. The comment is demonstrably wrong/obsolete and the code is correct as-is.

Do NOT remove TODOs that represent genuine future work, even if deferred.
