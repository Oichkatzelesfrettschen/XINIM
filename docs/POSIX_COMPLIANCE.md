# POSIX/SUSv5 Conformance Plan

This document outlines a staged plan to achieve a POSIX 2024 (SUSv5) compliant C API implemented in C++23 with a stable C ABI.

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
- Use mdoc manpages for function contracts; cross‑check with SUSv5.

