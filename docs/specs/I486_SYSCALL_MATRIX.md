# XINIM i486 Syscall Matrix

Date: 2026-03-17

## Summary

- **112 syscall numbers** defined in `include/xinim/sys/syscalls.h`
- **105+ implemented** in `src/kernel/i486/ring3.cpp`
- Syscall ABI: `int $0x80`, args in ebx/ecx/edx/esi/edi/ebp, return in eax
- Error convention: negative errno values (-1 to -131), compatible with dietlibc `cmp $-132,%eax`

## File I/O (3-15)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 3 | open | Impl | Relative path resolution via cwd |
| 4 | close | Impl | |
| 5 | read | Impl | Blocking on stdin, EINTR on signal |
| 6 | write | Impl | Console stdout/stderr, bootfs, ext2, pipes |
| 7 | lseek | Impl | |
| 8 | stat | Impl | Relative path resolution |
| 9 | fstat | Impl | |
| 10 | access | Impl | Relative path resolution |
| 11 | dup | Impl | |
| 12 | dup2 | Impl | |
| 13 | pipe | Impl | 16 pipes, 4096B buffer, O_NONBLOCK |
| 14 | ioctl | Impl | TCGETS/TCSETS, TIOCGWINSZ, TIOCGPGRP |
| 15 | fcntl | Impl | F_DUPFD, F_GETFD, F_SETFD, F_GETFL, F_SETFL |

## Directory Operations (16-24)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 16 | mkdir | Impl | bootfs + ext2 |
| 17 | rmdir | Impl | |
| 18 | chdir | Impl | Per-process cwd, validates directory |
| 19 | getcwd | Impl | Returns process->cwd |
| 20 | link | Stub | Returns -ENOSYS |
| 21 | unlink | Impl | bootfs + ext2 |
| 22 | rename | Impl | |
| 23 | chmod | Impl | Succeeds silently (single-user) |
| 24 | chown | Impl | Succeeds silently (single-user) |

## Process Management (25-33)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 25 | exit | Impl | Orphan reparenting, SIGCHLD, SA_NOCLDWAIT |
| 26 | fork | Impl | Copies address space, signal handlers, cwd |
| 27 | execve | Impl | Resets signals, closes FD_CLOEXEC |
| 28 | wait4 | Impl | WNOHANG, WUNTRACED, stopped children |
| 29 | getpid | Impl | |
| 30 | getppid | Impl | |
| 31 | kill | Impl | Send to PID, group, or broadcast |
| 32 | signal | Impl | SIG_DFL/SIG_IGN/handler |
| 33 | sigaction | Impl | SA_RESTART, SA_NODEFER, SA_RESETHAND, SA_NOCLDWAIT |

## Memory Management (34-37, 64)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 34 | brk | Impl | Heap growth/shrink with bounds checking |
| 35 | mmap | Impl | MAP_PRIVATE|MAP_ANONYMOUS only |
| 36 | munmap | Impl | |
| 37 | mprotect | Stub | Returns 0 (no-op) |
| 64 | mremap | Stub | Returns -ENOSYS |

## Time (42-45)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 42 | time | Impl | CMOS RTC + PIT ticks |
| 43 | gettimeofday | Impl | Real epoch seconds + microseconds |
| 44 | clock_gettime | Impl | CLOCK_REALTIME + CLOCK_MONOTONIC |
| 45 | nanosleep | Impl | Timer-tick based, EINTR with remaining |

## User/Group IDs (46-51)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 46-49 | getuid/geteuid/getgid/getegid | Impl | Returns 0 (root) |
| 50-51 | setuid/setgid | Impl | Returns 0 (no-op) |

## Compatibility (52-64)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 52 | alarm | Impl | Per-process timer, SIGALRM delivery |
| 53-54 | getrlimit/setrlimit | Impl | Returns RLIM_INFINITY |
| 55 | getrusage | Impl | Returns zeros |
| 56 | setsid | Impl | Returns pid |
| 57 | umask | Impl | Tracks file creation mask |
| 58 | nice | Stub | Returns 0 |
| 59-60 | setreuid/setregid | Impl | Returns 0 |
| 61 | rt_sigprocmask | Impl | Signal mask manipulation |
| 62 | getpgid | Impl | Per-process pgid |
| 63 | getdents | Impl | Per-fd directory path, bootfs + ext2 |

## Phase 2 Additions (65-80)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 65 | setpgid | Impl | Sets process group |
| 66 | getpgrp | Impl | Returns pgid |
| 67 | getsid | Impl | Returns pgid |
| 68 | uname | Impl | XINIM/0.1.0/i486 |
| 69-70 | symlink/readlink | Stub | -ENOSYS (no ext2 symlink) |
| 71-73 | fchdir/fchmod/fchown | Impl | |
| 74-75 | truncate/ftruncate | Impl | |
| 76-77 | readv/writev | Impl | Up to 16 iovecs |
| 78-79 | select/poll | Impl | Blocking with timeout |
| 80 | rt_sigreturn | Impl | Signal frame restore |

## Socket Syscalls (81-95)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 81 | socket | Impl | AF_INET, SOCK_DGRAM/SOCK_STREAM |
| 82 | bind | Impl | |
| 83 | listen | Impl | |
| 84 | accept | Stub | -ENOSYS |
| 85 | connect | Impl | UDP + TCP (simplified) |
| 86 | sendto | Impl | Dispatches to netstack UDP |
| 87 | recvfrom | Impl | Reads from socket rx buffer |
| 88 | shutdown | Impl | |
| 89-95 | setsockopt..socketpair | Stub | -ENOSYS |

## Phase 3 Additions (96-112)

| # | Name | Status | Notes |
|---|------|--------|-------|
| 96 | sigpending | Impl | Returns pending & ~blocked |
| 97 | sigsuspend | Impl | Atomic mask swap + suspend |
| 98 | getdents64 | Impl | 64-bit dirent compat |
| 99 | dup3 | Impl | O_CLOEXEC flag |
| 100 | procinfo | Impl | Dumps process table for ps |
| 101 | flock | Impl | Advisory (succeed silently) |
| 102-103 | fsync/fdatasync | Impl | Succeed silently |
| 104 | pipe2 | Impl | O_CLOEXEC/O_NONBLOCK |
| 105 | sched_yield | Impl | Yields quantum |
| 106 | gettid | Impl | Returns pid |
| 107 | statfs | Impl | ext2 filesystem info |
| 108 | lstat | Impl | Delegates to stat |
| 109-111 | openat/mkdirat/unlinkat | Impl | fd-relative paths |
| 112 | set_tid_address | Impl | Returns pid |
