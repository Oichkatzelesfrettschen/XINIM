# Xinim Test Coverage Matrix

Phase 7 update: 2026-02-26.

## Summary

| Label      | Count | Pass | Notes |
|------------|-------|------|-------|
| unit       | 25    | 25   | All host-side unit tests |
| crypto     | 5     | 5    | FIPS 202, NTT, Montgomery, constants, Kyber512 compile |
| scheduler  | 4     | 4    | Scheduler, edge cases, deadlock, service contract |
| sync       | 5     | 5    | Ticket spinlock, MCS, PhaseRWLock, capability mutex, lock manager |
| math       | 3     | 3    | Octonion, Fano multiplication, core types |
| kernel     | 8     | 8    | Wait graph x2, service manager x2, net driver stub, syscall dispatch, IPC channel, ELF parser |
| ipc        | 1     | 1    | Lattice IPC Channel push/pop/overflow/wrap |
| integration| 2     | N/A  | QEMU boot smoke test, kshell test (require built kernel) |

Total registered: 27 (25 unit + 2 integration).

## Host-Side Unit Tests (23)

| Test Name | Label(s) | Source | Status | Notes |
|-----------|----------|--------|--------|-------|
| test_wait_graph | unit;kernel | test/test_wait_graph.cpp | PASS | WaitGraph DAG, cycle detection |
| test_wait_graph_stress | unit;kernel | test/test_wait_graph_stress.cpp | PASS | Stress: 100 nodes, 1000 edges |
| test_scheduler | unit;scheduler | test/test_scheduler.cpp | PASS | CFS scheduler, process slots |
| test_scheduler_edge | unit;scheduler | test/test_scheduler_edge.cpp | PASS | Edge: empty queue, single proc, priority |
| test_scheduler_deadlock | unit;scheduler | test/test_scheduler_deadlock.cpp | PASS | Deadlock detection integration |
| test_service_contract | unit;scheduler | test/test_service_contract.cpp | PASS | Service contract rules |
| test_ticket_spinlock | unit;sync | test/test_ticket_spinlock.cpp | PASS | FIFO ordering, single-threaded |
| test_mcs_spinlock | unit;sync | test/test_mcs_spinlock.cpp | PASS | MCS basic lock/unlock (cooperative semantics) |
| test_phase_rwlock | unit;sync | test/test_phase_rwlock.cpp | PASS | PhaseRWLock RAII guards (cooperative semantics) |
| test_capability_mutex | unit;sync | test/test_capability_mutex.cpp | PASS | Capability-based mutex |
| test_lock_manager | unit;sync | test/test_lock_manager.cpp | PASS | Deadlock-detecting lock manager |
| test_octonion | unit;math | test/test_octonion.cpp | PASS | Octonion Cayley-Dickson multiplication |
| test_fano_multiply | unit;math | test/test_fano_multiply.cpp | PASS | Fano plane multiplication table |
| test_core_types | unit;math | test/test_core_types.cpp | PASS | Core type trait assertions |
| test_fips202 | unit;crypto | test/test_fips202.cpp | PASS | SHA3-256/512, SHAKE128/256 NIST KAT |
| test_ntt_roundtrip | unit;crypto | test/test_ntt_roundtrip.cpp | PASS | NTT forward/inverse round-trip |
| test_montgomery_reduce | unit;crypto | test/test_montgomery_reduce.cpp | PASS | Montgomery reduction identity |
| test_kyber_constants | unit;crypto | test/test_kyber_constants.cpp | PASS | Kyber768 parameter values and relationships |
| test_service_manager | unit;kernel | test/test_service_manager.cpp | PASS | ServiceManager legacy test |
| test_kyber_e2e | unit;crypto | test/test_kyber_e2e.cpp | PASS | Kyber512 constants (E2E pending kem sources) |
| test_service_manager_api | unit;kernel | test/test_service_manager_api.cpp | PASS | ServiceManager current API |
| test_net_driver_stub | unit;kernel | test/test_net_driver_stub.cpp | PASS | NetDriver stub returns failure values |
| test_syscall_dispatch | unit;kernel | test/test_syscall_dispatch.cpp | PASS | Syscall table constants and ranges |
| test_ipc_channel | unit;ipc;kernel | test/test_ipc_channel.cpp | PASS | Lattice IPC Channel FIFO, overflow, wrap-around |
| test_elf_parser | unit;kernel | test/test_elf_parser.cpp | PASS | ELF64 header validation, flags-to-prot conversion |

## Integration Tests (2, QEMU-required)

| Test Name | Label(s) | Source | Status | Notes |
|-----------|----------|--------|--------|-------|
| boot_smoke_test | integration;boot | test/boot/smoke_test.sh | N/A | Requires kernel image + QEMU |
| x86_64_shell_test | integration;kshell | test/boot/x86_64_shell_test.py | N/A | Requires kernel image + QEMU TCP:4555 |

## Untested Subsystems

| Subsystem | Reason | Phase |
|-----------|--------|-------|
| Kyber512 keygen/encap/decap full round-trip | Missing kem.cpp, poly.cpp, indcpa.cpp | Phase 7 |
| VFS server SYS_write full path | Minimal loop in bare_metal_stubs; full ramfs in Phase 7 | Phase 7 |
| ELF loader header validation | DONE (test_elf_parser) | Phase 7 complete |
| ELF loader full binary loading | Requires VFS + paging | Phase 8 |
| Signal handling | Requires process context | Phase 7 |
| virtio-net driver | Not yet implemented | Phase 7 |
| Syscall dispatch runtime | Requires ring-0 context | Phase 7 |
| IPC (lattice_ipc) | Requires process context | Phase 6 |
| Memory manager (src/mm/) | Requires kernel paging | Phase 7 |
| MCS spinlock multi-threaded | Cooperative-only design; hangs under preemption | Known limitation |
| PhaseRWLock multi-threaded | Cooperative-only design; hangs under preemption | Known limitation |

## Test Filtering

Run subsets with CTest labels:

```
# All unit tests
ctest --test-dir build/Debug -L unit

# Crypto tests only
ctest --test-dir build/Debug -L crypto

# Scheduler tests only
ctest --test-dir build/Debug -L scheduler

# Sync primitive tests only
ctest --test-dir build/Debug -L sync

# All tests including integration (requires kernel image)
ctest --test-dir build/Debug
```

## History

| Date | Total | Unit | Integration | Notes |
|------|-------|------|-------------|-------|
| 2026-02-26 | 22 | 20 | 2 | After Phase 2 build hardening |
| 2026-02-26 | 25 | 23 | 2 | After Phase 4: +kyber_e2e, +service_manager_api, +net_driver_stub, +syscall_dispatch |
| 2026-02-26 | 26 | 24 | 2 | After Phase 6: +test_ipc_channel (lattice Channel push/pop) |
| 2026-02-26 | 27 | 25 | 2 | After Phase 7: +test_elf_parser (ELF64 header validation) |
