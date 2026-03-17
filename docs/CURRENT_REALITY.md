# Current Reality

Date: 2026-03-10

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
- The i486 lane produces a GRUB/Multiboot2 ISO and reaches a supervised native
  Ring 3 `mksh` init service under QEMU.
- The i486 lane is currently verified on `pc-i440fx-10.2` and `isapc` with
  `-cpu 486`.
- The i486 init shell is linked against the repo-retargeted `dietlibc`.
- The i486 supervised init service respawns native `mksh` on clean exit instead
  of dropping into rescue.
- The i486 supervision path now synchronizes service identity, PID, state, and
  restart budgeting with the freestanding recovery DAG.
- The i486 lane now stages a second long-lived support-service payload,
  `/bin/holdsvc`, in the same supervision table as the init service.
- The i486 lane supports COM2 shell automation and a local VGA/keyboard tty
  path for the init shell.
- The i486 lane verifies an ATA-backed ext2 root path and reboot-persistent
  ext2 mutation for the focused QEMU 486 profiles.
- The hosted shell builds canonically as `xash`.

## Not Verified Today

- Any true timer-driven multi-service scheduler on the i486 lane
- Any concurrent execution of multiple long-lived Ring 3 services on the i486
  lane
- Any full POSIX.1-2008 shell compliance claim beyond the current bootstrap
  `mksh` contract
- Any runtime serialization stack based on MessagePack or Cap'n Proto

## Architectural Truth

- Each build tree configures exactly one CPU lane.
- Conan manages host-side build integration only. Freestanding kernel runtime
  code remains repo-owned.
- The 32-bit lane currently uses GRUB plus Multiboot2.
- The x86_64 lane currently uses Limine.
- `mksh` is the active i486 guest shell baseline.
- `xash` remains in-tree, but it is no longer the normal i486 init-shell path.
- The canonical scheduler policy lives in `UnifiedScheduler` plus
  `src/kernel/scheduler_policy.hpp`, but the final legacy bare-metal runnable
  selection path still has one remaining `pick_proc()` bridge.

## Doc Truth

- `docs/BUILD.md` is the canonical build guide.
- Older wrapper-script and external-sysroot flows are historical context, not
  current build truth.
- Generated analysis snapshots under `docs/analysis/tooling/` and
  `docs/analysis/ast_dependency_*.json` still record old paths because they are
  historical artifacts, not live guidance.
