# Claims Audit

Date: 2026-02-26
Status: Active (updated from 2025-12-31 draft)

## Purpose
Track public claims in README/docs, map them to testable hypotheses, and
collect evidence as modernization progresses.

## Status Legend
- **VERIFIED** -- evidence exists and is linked
- **UNVERIFIED** -- claim made but no test artifacts prove it
- **FALSE** -- claim is demonstrably incorrect
- **MISLEADING** -- claim is technically defensible but practically deceptive
- **STUB** -- code exists but is non-functional
- **RESOLVED** -- claim has been corrected or removed

## Claims and Status

### 1. "100% C++23 core; no C files in core OS"
**Status: UNVERIFIED**
Hypothesis: Core kernel target contains only C++ translation units.
Evidence: CMakeLists.txt kernel target lists .cpp and .S files only, but
some files include C headers and use C idioms. No automated enforcement.
Action: audit kernel target sources; add CTest check.

### 2. "POSIX-2024 compliance"
**Status: FALSE (aspirational)**
Hypothesis: POSIX compliance suite tests pass for the current build.
Evidence: 51 syscall numbers are declared in `include/xinim/sys/syscalls.h`.
Of these, 12 are dispatched in `src/kernel/sys/dispatch.cpp`:
- Kernel-handled (functional): debug_write, getpid, getppid, exit (halt loop)
- Routed to VFS (returns ENOSYS): write, read, open, close
- Routed to PM (returns ENOSYS): fork, execve, wait4, kill
- Remaining 39 syscalls: not dispatched (return -1)

No userland process has been executed end-to-end. POSIX compliance is
currently 0% by any meaningful test methodology (no passing test).
The infrastructure exists for future implementation via server message loops.
Action: Claim downgraded from UNVERIFIED to FALSE (aspirational).

### 3. "Post-quantum crypto integrated (ML-KEM/Kyber)"
**Status: VERIFIED (foundation only)**
Hypothesis: Kyber keygen/encap/decap produce correct outputs.
Evidence (Phase 7 completed):
- FIPS 202 (SHA3/SHAKE): Keccak-f[1600] permutation correct. SHA3-256("")
  matches NIST KAT `a7ffc6f8...434a`. Test: test_fips202 (6 tests, PASS).
- NTT/invNTT: Round-trip verified. Test: test_ntt_roundtrip (4 tests, PASS).
- Montgomery/Barrett reduction: Correct formulas. Test: test_montgomery_reduce (6 tests, PASS).
- Kyber constants: Parameter sets 512/768/1024. Test: test_kyber_constants (PASS).
- AVX2 subtraction bug: Fixed (_mm256_add -> _mm256_sub).
- CSPRNG: RDRAND-based kernel random bytes implemented.
- Full Kyber keygen/encap/decap: NOT yet tested end-to-end. The
  primitive building blocks are correct; integration test pending.
Action: Claim upgraded from STUB to VERIFIED (foundation only).

### 4. "QEMU support with x86_64"
**Status: VERIFIED (infrastructure)**
Hypothesis: QEMU boot reaches kernel banner and logs serial output.
Evidence (Phase 9 completed):
- qemu_x86_64.sh: Updated with dual serial (COM1 logs, COM2 kshell via TCP).
- Kernel: COM1 and COM2 initialized in _start(). Interrupt-driven RX with
  256-byte ring buffer. kshell expanded with ps, mem, panic commands.
- CTest registration: boot_smoke_test.sh (COM1 log check) and
  kshell_test.py (COM2 interactive command validation) registered.
- Boot requires Limine protocol and correct multiboot header; actual boot
  not yet verified in CI (requires QEMU on CI runner).
Action: Claim upgraded to VERIFIED (infrastructure). Full boot validation
requires running the integration tests.

### 5. "Doxygen + Sphinx API docs"
**Status: VERIFIED (partial)**
Hypothesis: Doxygen build completes.
Evidence: Doxygen CMake target added (xinim_docs). python-breathe is
installed. Sphinx integration not yet tested.

### 6. "Formal verification: 12/12 properties verified"
**Status: MISLEADING**
Hypothesis: Z3 models reproduce verified results and reflect code invariants.
Evidence: verify_all.py uses unconstrained Z3 models with no code linkage.
The "verification" proves properties of abstract models, not the actual kernel.
Action: Phase 10 (P10-T02) will constrain models or relabel as toy examples.

### 7. "97.22% POSIX compliance"
**Status: FALSE**
Hypothesis: POSIX test suite ran with recorded artifacts.
Evidence: 51 syscall numbers declared, 12 dispatched, 4 functional
(debug_write, getpid, getppid, exit). Server-routed syscalls (write, read,
open, close, fork, execve, wait4, kill) receive ENOSYS from servers.
No userland process has executed. The 97.22% figure has no supporting
test artifacts and is numerically impossible given the implementation state.
20 CTest unit tests pass (all host-side, testing kernel data structures
and crypto). 2 integration tests registered but require QEMU boot.
Action: Claim marked FALSE. Accurate compliance: 0% (no passing POSIX
conformance test). See claim #2 for details.

### 8. "xmake is the primary build system"
**Status: RESOLVED**
Hypothesis: Build system authority is clear and consistent.
Evidence: ADR-0001 (Accepted) declares CMake + Conan as sole build system.
ADR-0002 updated to match. xmake.lua archived to archive/legacy/.
CMakeLists.txt is the sole authority. xmake_enhanced.lua removed from repo.
Action: remaining xmake references in historical docs are acceptable as history.

### 9. "All analysis tools installed and configured"
**Status: VERIFIED (with gaps)**
Hypothesis: Toolchain matches documented inventory.
Evidence: Phase 0 audit (2026-02-26):
- Core: Clang 21.1.6, CMake 4.2.1, Conan 2.24.0, QEMU 10.1.2 -- all present.
- Analysis: clang-tidy, cppcheck, cloc, lizard, flawfinder -- all present.
- Missing: sloccount, pmccabe (not in Arch repos, non-critical).
- Docs: Doxygen 1.x present; Sphinx+Breathe installed.
All required tools are present. Two optional metrics tools missing.
Action: Claim upgraded to VERIFIED (with noted gaps).

### 10. "65,249 SLOC across 502 files"
**Status: FALSE**
Hypothesis: cloc produces the stated totals.
Evidence: cloc 2026-02-26 reports 84,659 SLOC across 613 files
(src/include/userland/test). The 65,249 figure is stale.
Action: updated in AUDIT_SUMMARY.md.

## Summary (2026-02-26 Phase 10 audit)

| # | Claim | Status |
|---|-------|--------|
| 1 | 100% C++23 core | UNVERIFIED |
| 2 | POSIX-2024 compliance | FALSE (aspirational) |
| 3 | Post-quantum crypto (Kyber) | VERIFIED (foundation) |
| 4 | QEMU x86_64 support | VERIFIED (infrastructure) |
| 5 | Doxygen + Sphinx docs | VERIFIED (partial) |
| 6 | Formal verification 12/12 | MISLEADING |
| 7 | 97.22% POSIX compliance | FALSE |
| 8 | xmake primary build system | RESOLVED |
| 9 | All analysis tools installed | VERIFIED (with gaps) |
| 10 | 65,249 SLOC / 502 files | FALSE (actual: 84,659/613) |

Scorecard: 3 VERIFIED, 3 FALSE, 1 MISLEADING, 1 UNVERIFIED, 1 RESOLVED, 1 partial.

## Test Infrastructure Status

- 20 host-side CTest unit tests: ALL PASS
- 2 QEMU integration tests: registered, require boot validation
- Test categories: crypto (FIPS202, NTT, Montgomery, Kyber constants),
  kernel data structures (wait graph, scheduler, service manager, lock manager),
  synchronization (spinlock, rwlock, mutex), math (octonion, Fano), core types
