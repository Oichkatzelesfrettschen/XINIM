# 100-Step Modernization Roadmap

Date: 2026-03-08
Status: Active

This roadmap is intentionally granular. It is a sequencing and tracking tool
for the active Conan + CMake build graph, low-RAM VFS consolidation, staged
shell/userland work, and truth-in-docs cleanup.

Legend:
- done: completed and verified in the active tree
- now: next executable tranche
- later: queued work after the active tranche

## Phase 1: Truth and Documentation

1. done - establish `docs/CURRENT_REALITY.md` as the short truth source
2. done - establish `docs/BUILD.md` as the canonical build guide
3. done - establish `docs/REQUIREMENTS.md` as the canonical requirements doc
4. done - build and refresh `docs/analysis/CLAIMS_AUDIT.md`
5. now - correct stale README claims that overstate POSIX or QEMU coverage
6. now - distinguish roadmap language from conformance language in POSIX docs
7. now - align external source indexes with current Open Group terminology
8. now - keep `docs/analysis/TODO_TRACKER.md` focused on live work only
9. done - add a verifier for stale "full" and "100%" claim phrases
10. later - add a verifier that canonical docs agree on build roots

## Phase 2: Canonical Build Flow

11. done - remove script wrappers as the canonical build front door
12. done - make Conan + CMake the authoritative build path
13. done - keep lane-specific trees under `build/<lane>/<config>`
14. done - make image/bootstrap outputs live under the active build tree
15. now - remove remaining active-doc references to `.xinim/...` roots
16. now - keep shell/test harnesses defaulting to `build/...`
17. later - audit helper scripts for stale build-root fallbacks
18. later - add a build-root consistency check target
19. later - align Sphinx references with the canonical build flow
20. later - align any generated helper docs that still imply wrapper scripts

## Phase 3: Requirements and Tooling Audit

21. done - verify core host tools on PATH
22. done - verify optional `i386-elf-gcc` and `i686-elf-gcc` presence
23. done - document Arch package baselines
24. done - document optional AUR cross-toolchain packages
25. now - document active boot-test Python harness requirements explicitly
26. now - keep xorriso called out as a hard host requirement
27. later - add a requirements-check script for core host tools
28. later - add a requirements-check script for optional cross lanes
29. later - record minimum verified tool versions from local runs
30. later - audit docs for package instructions that still assume apt-only

## Phase 4: Build Graph Coverage

31. done - add `xinim_audit_build_coverage`
32. done - generate build graph audit markdown/json outputs
33. done - continue reducing unwired `src/commands` (now zero deferred candidates
   in `src/commands`)
34. done - keep reducing unwired fs-related tests while keeping `userland` at
  zero unwired files
35. now - classify 25+ unwired `src/kernel/*` files into active/lane-specific/retired
   (latest pass moved 25 units to `archive/legacy/kernel`)
36. now - keep classifying unwired `src/mm/*` into active/lane-specific/retired
  while keeping the reconciled `dma.cpp` compatibility layer aligned with the
  active `memory.cpp` and `dma_allocator.cpp` lane, including 486-safe
  below-16MiB plus 64KiB-window constraints
37. now - keep promoting only compact kernel primitives like `pipe.cpp` when
  they can be covered with honest host stubs instead of shallow compile-only
  wiring
38. later - classify all unwired `src/fs/*` into mined/reference/archive
39. later - classify all unwired `src/tools/*` into hosted-tool/archive buckets
40. later - classify unwired `test/*` files into active/legacy/not-ready
41. later - publish a per-directory build-coverage burn-down report

## Phase 5: Hosted Commands

41. done - promote `echo`
42. done - promote `pwd`
43. done - promote `true` and `false`
44. done - promote `mkdir`, `rm`, `touch`, `cp`, `ls`
45. done - promote `cat`, `wc`, `ln`, `chmod`
46. done - promote `basename`, `head`, `env`, `sleep`
47. done - promote `rev` and `tee`
48. done - promote `cut` after warnings-as-errors cleanup
49. done - promote additional small standalone tools after `cut` with `rmdir`,
  `sum`, `tr`, `date`, `comm`, `sync`, `cmp`, `uniq`, and `od`
50. now - split shared command support into `xinim::tools::core`

## Phase 6: Hosted Command Validation

51. done - create hosted command smoke coverage
52. done - verify file creation and copy workflows
53. done - verify hard link and symlink workflows
54. done - verify `chmod`, `basename`, `head`, `env`, `sleep`
55. done - verify `rev` and `tee`
56. done - add `cut` smoke coverage when the target lands
57. done - add failure-path and transformation coverage for more hosted
  command argument parsing with `sum` and `tr`
58. done - add a command inventory doc showing active hosted tools
59. now - keep the command inventory triage list with an explicit 10-item deferred
  candidate set
60. later - add per-command references to source behavior goals
61. later - add one focused CTest per nontrivial hosted tool

## Phase 7: Filesystem Semantics

61. done - keep `src/vfs/*` as the active filesystem direction
62. done - mine `src/fs/*` semantics instead of carrying two live stories
63. done - integrate hosted permission semantics into fs host compat
64. done - wire fs regression tests for links, chmod, touch ops
65. done - wire fs regression tests for remove, copy_symlink, rename
66. done - modernize and wire the next aligned fs regression tests for
  `get_status`, `create_directory`, and `create_directories`
67. done - keep shrinking old `filesystem_ops`-centric test assumptions with
  modern `copy_file` and `change_ownership` coverage in the active graph
68. done - keep shrinking old `filesystem_ops`-centric assumptions with active
  `copy` coverage in the host graph
69. now - add a compact-VFS-to-hosted-fs semantic map doc and classify adjacent
  parallel kernel/mm stories
70. later - audit copy/remove/rename edge cases against POSIX wording

## Phase 8: Low-RAM VFS and Bootfs

71. done - make `vfs_profile=auto|default|tiny` first-class
72. done - prefer `tiny` on x86_32 and `default` on x86_64
73. done - add bounded positive/negative path-walk cache
74. done - add a `bootfs` hit/miss cache on the 32-bit path
75. done - promote `bootfs` into shared usage across x86_32 and x86_64
76. done - add a compact reclaimable vnode-like table
77. done - add chainable directory blocks instead of fixed 32-entry directories
78. now - keep the compact VFS path common across 32-bit and 64-bit lanes
79. later - add reclaim-pressure tests for tiny VFS behavior
80. later - add tighter memory accounting for the 4 MiB to 16 MiB target lane

## Phase 9: x86_64 Guest Path

81. done - restore repo-local Limine image generation
82. done - make x86_64 shell/control path boot under QEMU
83. done - validate staged shell interactions over COM2
84. now - keep x86_64 shell/control claims narrow and evidence-backed
85. later - extend staged shell handoff into richer process/userland flows
86. later - add more shell commands to x86_64 guest smoke coverage
87. later - validate rescue/continue flow under more boot timing variations
88. later - add x86_64 image layout checks into broader CI
89. later - re-evaluate device and graphics claims only after tests exist
90. later - carry low-RAM VFS evidence into x86_64 memory-pressure scenarios

## Phase 10: 32-bit Guest Lanes

91. done - i486 reaches Ring 3 `xash`
92. done - i586 reaches Ring 3 `xash`
93. done - i686 reaches Ring 3 `xash`
94. later - validate `x86_32_core2`
95. later - validate `x86_32_athlon`
96. later - validate `x86_32_phenom`
97. later - align shell/userland behavior across all 32-bit lanes
98. later - carry temp-environment and execve semantics across all lanes
99. later - validate low-RAM tiny-VFS behavior on 486-class settings
100. later - use the resulting stable baseline for broader POSIX shell work
