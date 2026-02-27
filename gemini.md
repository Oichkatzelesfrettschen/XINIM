# Xinim Gemini Notes

Version: 0.1
Status: Draft (active)

## Scope and Intent
Xinim is a research microkernel OS being modernized into a pure C++23 codebase
with a CMake + Conan build system. The modernization goals are to enforce
warnings-as-errors, enable deterministic builds, and validate QEMU boots with
repeatable logs. Documentation is expected to integrate with Doxygen + Sphinx
(Breathe).

## Current State (Sanity Check)
- Build system: CMake + Conan are authoritative per specs and AGENTS; new docs
  introduced xmake-primary guidance that now conflicts with project rules.
- Toolchain: C++23 required; Clang 18+ preferred (per docs/AGENTS.md).
- Dependencies: libsodium via Conan; limine headers vendored under
  third_party/limine; dietlibc and POSIX test suite are still vendored.
- Build blockers: missing or stale include paths (e.g., vm.h, minix headers)
  still prevent a clean CMake build.
- Docs: new specs exist, but legacy reports still describe xmake workflows.
- CMakeLists.txt currently states it was generated from xmake and uses pkg-config
  libsodium, which conflicts with Conan-based requirements.
- Formal verification scripts run unconstrained Z3 models; results are not yet
  linked to code or CI artifacts.

## Roadmap (Detailed)
1. Sync upstream and capture repository status.
2. Consolidate roadmap docs and publish a single authoritative plan.
3. Inventory C/C++/ASM files and build system files.
4. Record current standard flags and toolchain usage.
5. Map external dependencies and third_party status.
6. Audit TODO/FIXME comments and classify by subsystem.
7. Review docs for contradictions (xmake vs CMake/Conan).
8. Refresh requirements, specs, and acceptance criteria.
9. Update gemini/claude/agents docs to reflect modernization scope.
10. Finalize clang-tidy and clang-format policies for C++23.
11. Refactor CMake to target-based flags and strict standards.
12. Integrate Conan toolchain/presets for Debug/Release.
13. Add Doxygen + Sphinx build targets and verify Breathe config.
14. Fix missing includes and header layout issues.
15. Refactor C/C++ boundaries and update extern "C" usage.
16. Apply clang-format to modified C++ sources.
17. Resolve platform-specific code paths for portability.
18. Build Debug/Release locally; capture and fix compiler errors.
19. Expand unit and integration test coverage; register with CTest.
20. Add CI-friendly scripts or presets for deterministic builds.
21. Produce bootable image and run QEMU with log capture.
22. Add benchmarking harnesses for critical components.
23. Update README and build docs to remove xmake references.
24. Track remaining TODOs and open issues in docs/analysis.

## Open Risks
- Large third_party trees (dietlibc, POSIX tests) may not be C++23-ready.
- Build errors from missing headers may indicate inconsistent include layout.
- Documentation debt is significant; must be reconciled systematically.

## References
- docs/AGENTS.md
- docs/analysis/AUDIT_SUMMARY.md
- docs/analysis/ROADMAP_CONSOLIDATED.md
- docs/specs/PRODUCT_REQUIREMENTS.md
- docs/specs/TECHNICAL_SPEC.md
- docs/testing/TEST_STRATEGY.md
