# Hosted Command Inventory

Date: 2026-03-08
Status: Active

This inventory is the authoritative list of curated hosted command tools for the
current native build lane (`xinim_commands_hosted`).

## Active Hosted Tools

- `basename` (`src/commands/basename.cpp`)
- `cat` (`src/commands/cat_cpp23.cpp`)
- `chmod` (`src/commands/chmod.cpp`)
- `chown` (`src/commands/chown.cpp`)
- `cmp` (`src/commands/cmp.cpp`)
- `comm` (`src/commands/comm.cpp`)
- `cp` (`src/commands/cp.cpp`)
- `cut` (`src/commands/cut_cpp23.cpp`)
- `date` (`src/commands/date.cpp`)
- `df` (`src/commands/df.cpp`)
- `echo` (`src/commands/echo.cpp`)
- `env` (`src/commands/env_cpp23.cpp`)
- `false` (`src/commands/false_cpp23.cpp`)
- `head` (`src/commands/head.cpp`)
- `kill` (`src/commands/kill.cpp`)
- `ln` (`src/commands/ln.cpp`)
- `ls` (`src/commands/ls.cpp`)
- `mkdir` (`src/commands/mkdir.cpp`)
- `mv` (`src/commands/mv.cpp`)
- `od` (`src/commands/od.cpp`)
- `pwd` (`src/commands/pwd.cpp`)
- `rm` (`src/commands/rm.cpp`)
- `rmdir` (`src/commands/rmdir.cpp`)
- `rev` (`src/commands/rev.cpp`)
- `sleep` (`src/commands/sleep.cpp`)
- `sum` (`src/commands/sum.cpp`)
- `sync` (`src/commands/sync.cpp`)
- `tee` (`src/commands/tee.cpp`)
- `tr` (`src/commands/tr.cpp`)
- `touch` (`src/commands/touch.cpp`)
- `true` (`src/commands/true_cpp23.cpp`)
- `uniq` (`src/commands/uniq.cpp`)
- `wc` (`src/commands/wc.cpp`)

## Smoke-Level Verification

- `hosted_commands_smoke_test` validates command discovery for all active tools.
- Per-command host tests exist for:
  - `cut` parsing edge cases
  - `sum`/`tr` transformation and failure behavior
  - mined command metadata coverage (`test_mined_command_registry`,
    `test_mined_key_command_map`)
  - filesystem helpers (`create`, `copy`, `rename`, `chown`, `change_permissions`,
    `link`, `remove`)

## Deferred Command Candidates

These files are currently all either active or intentionally archived:

- None pending.

## Archived Command Candidates

The following 60 command files have been reviewed and explicitly archived into
`archive/legacy/commands/` for staged reintroduction:

- `archive/legacy/commands/awk_cpp23.cpp`
- `archive/legacy/commands/async_grep_cpp23.cpp`
- `archive/legacy/commands/cal.cpp`
- `archive/legacy/commands/cc.cpp`
- `archive/legacy/commands/chmem.cpp`
- `archive/legacy/commands/size.cpp`
- `archive/legacy/commands/split.cpp`
- `archive/legacy/commands/ps_cpp23.cpp`
- `archive/legacy/commands/pwd_cpp23.cpp`
- `archive/legacy/commands/constexpr_date_cpp23.cpp`
- `archive/legacy/commands/cat.cpp`
- `archive/legacy/commands/ar.cpp`
- `archive/legacy/commands/clr.cpp`
- `archive/legacy/commands/dd.cpp`
- `archive/legacy/commands/dosread.cpp`
- `archive/legacy/commands/echo_cpp23.cpp`
- `archive/legacy/commands/encrypt_cpp23.cpp`
- `archive/legacy/commands/grep.cpp`
- `archive/legacy/commands/getlf.cpp`
- `archive/legacy/commands/gres.cpp`
- `archive/legacy/commands/lpr.cpp`
- `archive/legacy/commands/make.cpp`
- `archive/legacy/commands/libpack.cpp`
- `archive/legacy/commands/libupack.cpp`
- `archive/legacy/commands/login.cpp`
- `archive/legacy/commands/mknod.cpp`
- `archive/legacy/commands/mkfs.cpp`
- `archive/legacy/commands/mount.cpp`
- `archive/legacy/commands/passwd.cpp`
- `archive/legacy/commands/pr.cpp`
- `archive/legacy/commands/mined.cpp`
- `archive/legacy/commands/mined_editor.cpp`
- `archive/legacy/commands/mined_editor_complex.cpp`
- `archive/legacy/commands/mined_final.cpp`
- `archive/legacy/commands/mined_library.cpp`
- `archive/legacy/commands/mined_main.cpp`
- `archive/legacy/commands/mined_main_complex.cpp`
- `archive/legacy/commands/mined_simple.cpp`
- `archive/legacy/commands/mined_unified.cpp`
- `archive/legacy/commands/pr_modern.cpp`
- `archive/legacy/commands/roff.cpp`
- `archive/legacy/commands/shar.cpp`
- `archive/legacy/commands/sh1.cpp`
- `archive/legacy/commands/sh3.cpp`
- `archive/legacy/commands/sh4.cpp`
- `archive/legacy/commands/sh5.cpp`
- `archive/legacy/commands/simd_wc_cpp23.cpp`
- `archive/legacy/commands/test_mined_console.cpp`
- `archive/legacy/commands/sort.cpp`
- `archive/legacy/commands/sort_modern.cpp`
- `archive/legacy/commands/stty.cpp`
- `archive/legacy/commands/su.cpp`
- `archive/legacy/commands/svcctl.cpp`
- `archive/legacy/commands/tail.cpp`
- `archive/legacy/commands/time.cpp`
- `archive/legacy/commands/tar.cpp`
- `archive/legacy/commands/tar_modern.cpp`
- `archive/legacy/commands/umount.cpp`
- `archive/legacy/commands/update.cpp`
- `archive/legacy/commands/x.cpp`

## Triage Rule

- Add a candidate only when it compiles under warnings-as-errors with no external
  assumptions, is covered by a smoke test, and has a clear lane role.
