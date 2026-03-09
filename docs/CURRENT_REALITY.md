# Current Reality

Date: 2026-03-08

This file is the short checkpoint for what the repository actually does today.

## Verified Today

- The canonical build flow is pure Conan + CMake rooted in
  `build/<lane>/<config>`.
- `conan install` plus `cmake --preset ...` works for the active x86_64 and
  32-bit lanes.
- The active lane matrix is:
  - `x86_64`
  - `i486`
  - `i586`
  - `i686`
  - `x86_32_core2`
  - `x86_32_athlon`
  - `x86_32_phenom`
- 32-bit ISO generation now bootstraps a pinned repo-local GNU GRUB under the
  active build tree and uses that local `grub-mkrescue` by default.
- `xorriso` is still a host prerequisite for ISO assembly.
- The x86_64 image path uses a repo-local Limine bootstrap driven by CMake.
- The x86_64 image path reaches the staged shell over COM2 under QEMU, and the
  current shell smoke test can execute `help`, `pid`, `pwd`, `ls /bin`,
  `cat /etc/motd`, `env PATH`, `cp`, and `command -v xash`.
- The i486 lane produces a GRUB/Multiboot2 ISO and reaches a Ring 3 `xash`
  shell on COM2 under QEMU.
- The hosted shell builds canonically as `xash`.

## Not Verified Today

- Any production graphics path beyond metadata handoff and early console work
- Any full POSIX.1-2008 shell compliance claim beyond the current bootstrap
  `xash` contract
- Any runtime serialization stack based on MessagePack or Cap'n Proto

## Architectural Truth

- Each build tree configures exactly one CPU lane.
- Conan manages host-side build integration only. Freestanding kernel runtime
  code remains repo-owned.
- The 32-bit lane currently uses GRUB plus Multiboot2.
- The x86_64 lane currently uses Limine.
- `xash` is the active shell baseline for both hosted and 32-bit guest bring-up.

## Doc Truth

- `docs/BUILD.md` is the canonical build guide.
- Older wrapper-script and external-sysroot flows are historical context, not
  current build truth.
- Generated analysis snapshots under `docs/analysis/tooling/` and
  `docs/analysis/ast_dependency_*.json` still record old paths because they are
  historical artifacts, not live guidance.
