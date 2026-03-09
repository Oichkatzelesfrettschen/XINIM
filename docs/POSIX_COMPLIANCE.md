# POSIX Conformance Roadmap

This document is a roadmap, not a claim of present conformance.

XINIM's verified state as of 2026-03-08 is a bootstrap shell and syscall path
that supports targeted guest and hosted validation. That is still far short of
full POSIX.1-2008 shell behavior or Issue 8 / POSIX.1-2024 Shell and Utilities
conformance.

The standards target is therefore split into two layers:
- near-term shell behavior target: POSIX.1-2008 shell command language for
  `xash`
- long-term API and utility target: Issue 8 / POSIX.1-2024 conformance where
  the repository has evidence, tests, and implementation coverage to support it

## Principles
- C ABI: all exported symbols use `extern "C"` and C data layouts.
- C++23 implementation: use RAII and types internally; no exceptions across ABI.
- Headers: strictly C17‑compatible where required; kernel headers are separate.

## Phases
- Phase 1: Processes, files, directories, time
  - exec/exit/wait, fork/vfork (or posix_spawn), open/read/write/close, lseek, fcntl, stat, chdir, gettimeofday/clock_gettime, errno.
- Phase 2: Signals, pty/tty, sockets
  - sigaction/sigset, termios, AF_INET/AF_INET6 sockets (TCP/UDP), select/poll.
- Phase 3: Threads and synchronization
  - pthreads (mutex/cond/rwlock), TLS, sched, clock_nanosleep, aio (optional).

## Strategy
- Syscall layer: thin C ABI into kernel syscall dispatcher.
- Libc layout: `libc/include` (C headers), `libc/src` (C++ sources exposing C symbols).
- Validation: compile `posix-testr` subsets; write conformance tests under `tests/posix`.

## Notes
- Wide‑char, locales, complex math: later milestones.
- Use mdoc manpages for function contracts; cross-check with the Open Group
  Issue 8 publications and the POSIX.1-2008 shell language text.

