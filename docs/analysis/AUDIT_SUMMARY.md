# XINIM Repository Audit Summary

Date: 2026-01-05
Scope: Roadmaps, build system, requirements, claims, and tooling docs.

## Structure Overview
- .github/workflows: CI and analysis automation.
- cmake/: CMake helper modules.
- conan/: Conan profiles and packaging.
- docs/: specs, roadmaps, reports, and guides.
- include/: public headers.
- libc/, third_party/: vendored dependencies (dietlibc, limine, etc).
- scripts/: build, cleanup, and boot helpers.
- src/: kernel, servers, drivers, fs, mm, userland.
- test/: unit, integration, and POSIX tests.
- tools/: analysis automation scripts.
- specs/: formal verification artifacts.

## Key Contradictions and Risks
1. Build system authority:
   - docs/AGENTS.md, docs/specs, and README.md require CMake + Conan only.
   - New docs (EXECUTIVE_SUMMARY, BUILD_SYSTEM_MODERNIZATION, ADR-0002, TOOLING_GUIDE)
     declare xmake primary.
   - Action: normalize all docs to CMake + Conan and retire xmake references.
2. CMakeLists mismatch:
   - Current CMakeLists.txt says "generated from xmake" and uses pkg-config libsodium.
   - conanfile.py and CMakePresets.json expect Conan toolchain integration.
   - cmake/ProjectOptions.cmake and cmake/CompilerWarnings.cmake are not wired.
   - Action: replace with Conan-driven, target-based CMake and remove xmake assumptions.
3. CI policy drift:
   - .github/workflows/analysis.yml emits warnings instead of failing on format issues.
   - Project rule: treat warnings as errors.
   - Action: enforce failure on lint/format/complexity thresholds or document exceptions.
4. Claims vs evidence:
   - POSIX compliance claims lack test artifacts or CTest integration evidence.
   - Formal verification claims are based on unconstrained Z3 models, not code-linked.
   - "All tools installed" claims are unverified in this environment.
   - External references confirm POSIX.1-2024 Issue 8 and FIPS 203 ML-KEM, but
     repo-level compliance remains unverified.
   - cloc 2026-02-26: 84,659 SLOC across 613 files (src/include/userland/test)
     across 605 files; earlier 65,249/502 figures are stale.
   - Action: update claims audit with testable hypotheses and evidence collection steps.
5. Roadmap fragmentation:
   - Roadmap docs conflict on build system and compiler version (C++23 vs Clang++17).
   - Action: consolidate roadmaps and mark legacy docs as non-authoritative.
6. Dependency docs drift:
   - REQUIREMENTS/TOOL_INSTALL list packages without per-module mapping and differ
     from tooling guide assumptions.
   - Action: add module-level requirements and align versions and sources.
7. Linting policy mismatch:
   - .clang-tidy enforces camelBack naming for functions/variables.
   - docs/STYLE_GUIDE.md and ADR-0002 require snake_case.
   - Action: align clang-tidy naming rules with documented style guide.

## Missing or Unclear Configurations
- Doxygen: CI checks for Doxyfile in repo root, but docs/Doxyfile is used.
- CMake presets: CMakePresets.json references Conan toolchain, CMakeLists.txt does not.
- QEMU scripts: scripts/qemu_x86_64.sh defaults to build/xinim, while presets use build/Debug.
- Some docs/scripts still assume xmake outputs.

## Immediate Warnings to Treat as Errors
- Git CRLF warning on include/xinim/fs/extent.hpp (line endings).
- Stash reconciliation needed for CMakeLists.txt (Conan-based vs xmake-generated).
- Tool inventory check shows sloccount and pmccabe missing (see tools_misc/tool_versions_2026-01-05.txt).
- Attempted pacman install for sloccount/pmccabe; sudo password required, install not completed.

## TODO/FIXME Inventory (Code)
- Total TODO/FIXME/XXX hits in src/include/userland/test: 176
- Hotspots by top-level folder:
  - src/kernel: 70
  - src/vfs: 33
  - src/servers: 26
  - src/drivers: 15
  - src/mm: 8
  - src/commands: 7
  - src/block: 7

## Re-Audit Plan (Next Pass)
1. Expand TODO scan with file-level mapping and priority tagging.
2. Verify tool availability and versions (cmake, conan, clang, doxygen, sphinx).
3. Validate POSIX test integration in CTest.
4. Verify QEMU boot steps and log outputs.
