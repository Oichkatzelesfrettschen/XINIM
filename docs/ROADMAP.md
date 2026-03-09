# Roadmap

Date: 2026-03-08
Status: Active

This roadmap is scoped to the repository as it exists now.

## Phase 1: Pure Conan + CMake

Status: Landed

Done:
- Canonical flow is now `conan install` plus `cmake --preset`.
- Build trees are lane-specific under `build/<lane>/<config>`.
- Images, logs, and bootstrapped tools live under the active build tree.
- The wrapper-script flow is no longer the canonical path.

Remaining:
- Remove lingering legacy-script references from secondary and historical docs.
- Keep CI and helper utilities aligned with the preset-owned build trees.

## Phase 2: Repo-Local Boot Tooling

Status: Landed for current boot lanes

Done:
- x86_64 image generation bootstraps Limine locally through CMake.
- 32-bit ISO generation bootstraps a pinned GNU GRUB locally through CMake.
- `xorriso` is injected explicitly into both image-generation paths.

Remaining:
- Keep the local tool cache deterministic and documented.
- Decide later whether more host tools should also move into repo-local
  bootstraps.

## Phase 3: 32-bit x86 Bring-Up

Status: Active

Done:
- `i486` boots under QEMU and reaches a Ring 3 `xash` shell.
- The lane architecture has been extended through `i586`, `i686`,
  `x86_32_core2`, `x86_32_athlon`, and `x86_32_phenom`.
- Per-lane prepare, boot, layout, and shell tests now hang off CTest when the
  image target exists.

Remaining:
- Expand the guest validation matrix across the full 32-bit lane set on every
  supported QEMU CPU model.
- Grow the 32-bit user ABI beyond the current bootstrap shell contract.
- Continue early-console and framebuffer work without regressing the 486-safe
  baseline.

## Phase 4: xash and Native Tools

Status: Active

Done:
- `xash` is the canonical hosted shell output.
- The guest bootstrap lane now reaches `xash` as PID 1.

Remaining:
- Drive `xash` toward POSIX.1-2008 behavior.
- Build out `xinim::tools::core` and the next native C++ tools on top of it.
- Treat Heirloom, ash, mksh, and bash as behavior references, not shipped
  implementation sources.

## Phase 5: Future Architecture Decisions

Status: Tracked

Open tracks:
- serialization direction for future hosted/userland protocols:
  [ADR 0010](adr/0010-serialization-track-msgpack-vs-capnp.md)
- remaining warning-debt burn-down
- deeper x86_64 guest validation and shell reachability

## Canonical References

- [BUILD.md](BUILD.md)
- [CURRENT_REALITY.md](CURRENT_REALITY.md)
- [analysis/TODO_TRACKER.md](analysis/TODO_TRACKER.md)
- [adr/0003-cmake-conan-build-system.md](adr/0003-cmake-conan-build-system.md)
