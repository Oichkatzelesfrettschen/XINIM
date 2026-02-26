# XINIM Consolidated Roadmap (Authoritative)

Date: 2026-01-05
Status: Active

## Sources Consolidated
- docs/ROADMAP.md
- docs/IMPLEMENTATION_ROADMAP_TRACKER.md
- docs/PHASE2_ULTRA_GRANULAR_ROADMAP.md
- docs/WEEKS10-12_COMPREHENSIVE_ROADMAP.md
- docs/EXECUTIVE_SUMMARY.md
- docs/TECHNICAL_DEBT_COMPREHENSIVE_REPORT.md
- docs/specs/PRODUCT_REQUIREMENTS.md
- docs/specs/TECHNICAL_SPEC.md

## Guiding Constraints (Non-Negotiable)
- Build system: CMake + Conan only (per docs/AGENTS.md and specs).
- Language: C++23 only; Clang 18+ preferred.
- Warnings treated as errors across build, lint, and CI.
- All modified C++ files include Doxygen comments.
- Generated artifacts stay out of git (build/, dist/, docs output).

---

## Phase 0: Alignment and Audit (Week 0-1)
1. Resolve build system contradictions in docs and scripts (remove xmake references).
2. Replace xmake-generated CMakeLists.txt with Conan-integrated, target-based CMake.
3. Align CMakePresets.json with Conan toolchain and repo requirements.
4. Update CI workflow to fail on format/lint/complexity issues (no warnings-only).
5. Update docs/REQUIREMENTS.md with per-module dependency mapping.
6. Update claims audit with testable hypotheses and evidence steps.
7. Record audit artifacts in docs/analysis (roadmap, claims, audit summary).
## Phase 1: Build and Toolchain Sanity (Week 1-2)
1. Verify tool versions (cmake, conan, clang, lld, doxygen, sphinx, qemu).
2. Run conan profile detect and conan install for Debug and Release presets.
3. Configure and build with CMake presets; resolve build errors and warnings.
4. Ensure libsodium and limine are wired through Conan/CMake targets.
5. Run QEMU boot scripts and capture logs under logs/.
6. Document reproducible build and boot steps with exact commands.

## Phase 2: Critical Blockers and TODOs (Week 2-4)
1. Resolve critical TODOs in drivers (E1000, AHCI) and VFS permission checks.
2. Implement IRQ management and timing calibration where referenced as blockers.
3. Fix missing or stale include paths in kernel and userland (vm.h, minix headers).
4. Integrate MM and VFS TODOs flagged in docs/PHASE2_SCOPE.md.
5. Reconcile toolchain build scripts with current CMake flow.
## Phase 3: POSIX and Userland Integration (Week 4-8)
1. Complete execve, signal handling, and memory mapping paths.
2. Wire POSIX test suites into CTest with labeled targets.
3. Add boot-time smoke tests for userland shells and IPC.
4. Validate POSIX compliance claims with recorded test outputs.
5. Expand logging for kernel services and IPC traces.

## Phase 4: Complexity and Error-Handling Refactors (Week 8-12)
1. Refactor CCN > 40 functions to std::expected-based flows.
2. Extract helpers to reduce CCN below 15 and file sizes below 1000 lines.
3. Replace raw owning pointers with RAII or smart pointers.
4. Add unit tests for refactored paths and concurrency edge cases.
## Phase 5: Verification, Documentation, and Release Readiness (Week 12+)
1. Align formal verification scripts with actual code invariants and inputs.
2. Run Doxygen and Sphinx builds and link API references via Breathe.
3. Add reproducible analysis reports under docs/analysis/reports/.
4. Prepare release checklist (build, test, boot, docs, compliance).

## Deliverables and Metrics
- Clean CMake + Conan build with warnings-as-errors.
- QEMU boot logs captured and reproducible.
- POSIX compliance tests integrated and reported.
- Claims audit updated with verified evidence and links.
- Consolidated roadmap replaces conflicting legacy guidance.

## Legacy Roadmap Crosswalk
- docs/ROADMAP.md: update to point here and remove Clang++17 references.
- docs/IMPLEMENTATION_ROADMAP_TRACKER.md: align build-system section to CMake.
- docs/PHASE2_ULTRA_GRANULAR_ROADMAP.md: keep as historical detail, mark legacy.
- docs/WEEKS10-12_COMPREHENSIVE_ROADMAP.md: fold tasks into Phases 2-4.
