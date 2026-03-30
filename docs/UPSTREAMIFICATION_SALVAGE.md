# Upstreamification Salvage Notes

This note captures the local-only items worth revisiting while the repo is
being realigned to `origin/main`.

## Keep And Evaluate

- `include/errno.h`
  Local errno surface expansion may still be useful, but it should be compared
  against the current upstream libc/POSIX direction before porting.

- `include/stddef.h`
  Small libc header additions look portable and low risk if they still match
  the current header model upstream uses.

- `include/string.h`
  Similar to `stddef.h`: likely salvageable in small, reviewable pieces rather
  than as a wholesale copy.

- `include/xinim/libcxx_config.hpp`
  This looks like a real attempt to formalize the hosted/freestanding C++
  configuration boundary and may still be valuable once reconciled with the
  Conan + CMake lane model now used upstream.

- `userland/shell/mksh/integration/xinim_job_control.c`
  There is likely useful shell/job-control behavior here, especially because
  upstream is actively evolving staged shell test registration.

## Probably Useful, But Re-port Selectively

- `src/kernel/modern_tty.cpp`
- `src/kernel/modern_tty.hpp`
- `src/kernel/uart_16550.cpp`
- `src/kernel/uart_16550.hpp`
- `src/kernel/qemu_hal.cpp`
- `src/kernel/qemu_hal.hpp`
- `src/kernel/nvme_driver.cpp`

These looked like genuine subsystem work in the old branch, but they should be
ported feature-by-feature onto upstream rather than revived wholesale from the
WIP branch.

## Do Not Carry Forward As-Is

- `394663c7` merge commit
  This mostly represents old `origin/master` history and should not be treated
  as a reusable unit.

- Generated/build artifacts
  `build/`, `build2/`, `build_complete/`, `.xmake/`, local binaries, and cache
  files should stay out of Git.

- Backup and duplicate files
  `cleanup_backups/`, `*_tmp.bak/`, `*_legacy.bak/`, `* 2`-style duplicates,
  and `.DS_Store` files are local debris, not migration inputs.

- Old xmake-centric scaffolding
  Upstream is now centered on Conan + CMake lane builds. Any old xmake-oriented
  workflow should only survive if reintroduced intentionally and without
  conflicting with that build layout.
