# XINIM TODO Tracker

Date: 2026-01-05
Status: Active

## Alignment and Audit
- [x] Create audit summary (docs/analysis/AUDIT_SUMMARY.md).
- [x] Create consolidated roadmap (docs/analysis/ROADMAP_CONSOLIDATED.md).
- [x] Expand claims audit with new hypotheses.
- [x] Add per-module requirements to docs/REQUIREMENTS.md.
- [ ] Reconcile CMakeLists.txt with Conan presets and specs.
- [ ] Remove or archive xmake-primary guidance across docs and scripts.
- [ ] Align CI workflows with warnings-as-errors enforcement.
- [ ] Align clang-tidy naming rules with docs/STYLE_GUIDE.md and ADR-0002.

## Build and Toolchain
- [ ] Verify tool versions (cmake, conan, clang, lld, doxygen, sphinx, qemu).
- [ ] Install missing tools (sloccount, pmccabe) once sudo is available.
- [ ] Run conan install for Debug/Release presets and document steps.
- [ ] Build via CMake presets and resolve warnings as errors.
- [ ] Confirm libsodium/limine dependency wiring through Conan.

## Critical Blockers
- [ ] Resolve E1000 driver TODOs (descriptor rings, TX/RX, DMA buffers).
- [ ] Resolve AHCI driver TODOs (structures, DMA, error handling).
- [ ] Implement VFS permission checks noted in docs/PHASE2_SCOPE.md.
- [ ] Implement IRQ management and timing calibration blocks.

## POSIX and Tests
- [ ] Integrate POSIX tests into CTest with labels.
- [ ] Validate POSIX compliance claims with captured test logs.
- [ ] Add boot-time smoke tests for shell and IPC.

## Documentation and Verification
- [x] Update gemini.md references once consolidation is complete.
- [ ] Align Doxygen/Sphinx paths with CI expectations.
- [ ] Link formal verification scripts to code invariants or mark as illustrative.
- [ ] Update README build system claims after CMake cleanup.
