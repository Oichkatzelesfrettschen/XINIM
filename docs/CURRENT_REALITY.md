# Current Reality

Date: 2026-08-16

This file is the short checkpoint for what the repository actually does today.

## i486 Lane -- Full-System Bring-Up Status

The i486 lane has working full-system process, filesystem, terminal, utility,
and initial networking infrastructure.

### Kernel (ring3.cpp ~4040 lines)

- **112 syscall numbers** defined, **105+ implemented**
- Signal delivery: sigaction, kill, sigreturn, Ctrl+C/Ctrl+Z, alarm, SIGCHLD,
  SA_NOCLDWAIT, EINTR on interrupted blocking syscalls
- Job control: WUNTRACED, ProcessState::Stopped, SIGSTOP/SIGCONT
- I/O multiplexing: select, poll with blocking and timeout
- Directory listing: getdents/getdents64 with per-fd directory path tracking
- Vectored I/O: readv, writev
- Real time: CMOS RTC + PIT ticks (time, gettimeofday, clock_gettime)
- Per-process working directory with relative path resolution
- 8 process slots, 4 MB user address space per process, 256 MB QEMU RAM
- 32 fd table, 256-byte paths, 16 pipes at 4096 bytes
- O_NONBLOCK enforcement on pipes (-EAGAIN/-EPIPE)
- Proper errno returns throughout (ENOENT, EBADF, EFAULT, ECHILD, EINTR, etc.)
- ANSI CSI escape sequence parsing (colors, cursor, clear screen)
- Termios ioctls (TCGETS/TCSETS, TIOCGWINSZ, TIOCGPGRP)
- ext2 timestamps (atime/ctime/mtime) on create and write
- ext2-backed `/bin`, `/etc`, and `/usr` canonical paths, with `/persist`
  mapped to the persistent root
- Anonymous `mmap`, `munmap`, and minimal moving `mremap` for dietlibc/TCC
  allocation paths
- execve resets signal handlers, closes FD_CLOEXEC descriptors
- Orphan reparenting to PID 1

### Networking

- **DMA page allocator**: bump allocator over Multiboot2 free memory above 4MB
- **virtio-net driver**: PCI discovery, feature negotiation, MAC address,
  DMA-backed virtqueue ring allocation, TX/RX paths, DRIVER_OK set
- **PCI subsystem**: initialized in i486 main.cpp
- **QEMU**: virtio-net-pci device attached
- **Socket syscalls**: native x86_64 AF_INET loopback datagrams cover socket,
  bind, connect, getsockname/getpeername, send/receive, select readiness,
  dup/dup2, fork/exec close-on-exec, shutdown, and teardown/error paths
  (syscalls 81-95; unsupported families and stream operations remain explicit
  errors)
- **Next**: lwIP integration and native TCP/IP stream semantics

### Utilities (68 total: 52 in bin/ + 16 in tests/)

**Tier 1 -- Shell essentials:**
cat, echo, true, pwd, env, cp, rm, test/[, basename, dirname, printf

**Tier 2 -- Text processing:**
wc, head, tail, sort, uniq, cut, tr, tee, grep (fixed-string), sed (s///),
awk (minimal: $N, NR, NF, -F, print)

**Tier 3 -- System utilities:**
kill, sleep, uname, id, touch, ln, xargs, dd, seq, yes, nproc, which,
chmod, chown, readlink, mktemp, expr, ps, date, df, du, hexdump

**Tier 4 -- Development/packaging:**
md5sum (RFC 1321), tar (ustar extract), find (recursive glob), diff, patch,
awk

**Test programs:**
signal_test, printf_test, forkexec_test, plus 13 existing test utilities

### In-Guest Toolchain

- `mksh` is the active interactive shell.
- The supervised shell receives `PS1` directly and does not auto-source
  `/etc/mkshrc` during the ring3 launch path.
- TCC is built as `/bin/tcc` for the 32-bit guest image when
  `XINIM_X86_32_BUILD_TCC=ON` (default).
- TCC runtime files are staged under `/usr/lib` and `/usr/lib/tcc`.
- The current TCC runtime is intentionally tiny: a XINIM-native `crt1.o`,
  empty `crti.o`/`crtn.o`, a minimal `libc.a` shim, and `libtcc1.a`.
- The verified compiler command uses XINIM's load base:
  `tcc -static -Wl,-Ttext=0x00400000 -o /persist/a.out /persist/a.c`.
- bmake is built as `/bin/bmake` when `XINIM_X86_32_BUILD_BMAKE=ON`
  (default).

### Build System

- 76 GRUB module2 entries for bootfs
- Loop-based CMake build for all dietlibc-linked utilities
- ext2 ATA disk with /etc/profile, /etc/mkshrc
- mksh rebuilt with HAVE_SELECT, HAVE_GETSID, HAVE_FTRUNCATE, HAVE_SIG_T,
  HAVE_FLOCK enabled
- Dynamic VMDK and qcow2 boot disks stage `/bin/tcc`, `/bin/bmake`,
  `/usr/include`, and the TCC runtime.
- The i686 kernel remains CMOV-capable (`pentium3`), while the guest userland
  runtime is built with conservative i486 code generation pending i686 ring3
  preemption/context RCA.

## x86_64 Lane

The supported modern boot lane is the Limine x86_64 image on QEMU 11.1.0 with
`-machine pc-q35-11.1`, `-cpu qemu64`, and `-smp 1`. The exact external gate
boots real Ring 3 PID 1 `/bin/sh` and completes inside a hard 60-second
timeout. A fallback shell, Ring 0 substitute, fake marker, enlarged bootfs,
weakened assertion, or timeout masking is not part of this result.

The exact gate passes 128 shell-grammar cases, the printf utility cases,
command-substitution alias-boundary cases, 70 repeated process lifecycles, and
40 repeated pipeline lifecycles. The finite conformance ledgers report:

```text
shell grammar:     125 closed,   0 open
token recognition: 17 closed,   0 open
shell language:     27 closed,  43 open
SUSv4 Issue 7 utilities: 2 closed, 173 open
system prerequisites: 7 closed, 0 open
Base Definitions:    0/95 parent, 0/1,483 clause rows
System Interfaces:   0/1,195 parent, 0/15,740 clause rows
expanded standards:  0/18,513 rows
active recursive frontier: 31 open parents, 2,644 open assertions
```

The grammar and token ledgers are subordinate evidence for shell-language
parents. Their row counts must not be added to the parent rows as if all rows
were independent. The utility and shell-language rows remain separate
denominators.

The first 31 open shell-language parents now have a recursive source-bound
frontier in `docs/posix/posix_shell_frontier_ledger.tsv` and
`docs/posix/posix_shell_frontier_requirements.tsv`: 724 ordered Chapter 2
assertions, 31 exact Q35 probe IDs, and 31 open parents. The probe cases are
coverage evidence, not parent closure; bounded Issue 7 remains 29/245.

The next queue slice is retained in
`docs/posix/posix_issue7_recursive_frontier_ledger.tsv` and
`docs/posix/posix_issue7_recursive_frontier_requirements.tsv`: 12 remaining
shell-language parents, 19 utility parents, and 2,644 open recursive
assertions. Its 31 exact Q35 probe IDs are executable coverage evidence; no
parent or recursive assertion is closed by the snapshot, and the bounded
Issue 7 count remains 29/245.

The staged x86_64 runtime has one shell implementation and one libc provider.
One mksh R59c source build uses the upstream legacy POSIX profile and is
installed byte-identically as `/bin/mksh` and `/bin/sh`; no `/bin/lksh` or
`/bin/xash` is staged. Every `/bin` ELF is either explicitly dietlibc-linked or
one of the finite syscall-only assembly programs. The image build rejects
unknown binaries, alternate shells, dynamic loaders, shared-library
dependencies, and missing dietlibc provenance.
The generated provider manifest also binds every staged `/bin` executable to
the exact build artifact that CMake copied into the image.

### Bounded progress estimates

These percentages are engineering estimates for the paused objective, not
conformance claims:

- Real x86_64 shell boot and core integration: approximately 90 percent. The
  exact platform gate passes; the remaining work is primarily semantics and
  breadth rather than reaching PID 1.
- Shell-language conformance: approximately 39 percent. Grammar and token
  recognition are closed, while 43 of 70 parent language rows remain open.
- Complete SUSv4 Issue 7 Shell and Utilities conformance: approximately 20 percent.
  `printf` and `true` are closed in the 175-utility denominator; the remaining
  utility rows require their own source-derived subledgers and witnesses.
- The full requested operating-system objective: approximately 15 percent.
  This includes the remaining shell, utility, filesystem, process, signal,
  terminal, modern-driver, and C++23 migration work.

Driver completion is not assigned a stronger percentage because the
repository does not yet carry a finite QEMU-device driver denominator. QEMU
source paths and model details are retained in
`docs/external_sources/QEMU_X86_PC_SOURCES.md`; guest drivers remain clean-room
implementations rather than copied QEMU code.

Project-owned implementation work uses C++23. Pinned mksh R59c and dietlibc
0.35 sources retain upstream C provenance. C headers remain only at genuine C,
assembly, firmware, libc, or external ABI boundaries and must remain valid C23
where they are C-only.

## Verified Today

- The canonical build flow is pure Conan + CMake rooted in
  `build/<lane>/<config>`.
- `conan install` plus `cmake --preset ...` works for all lanes.
- The i486 lane produces a bootable GRUB ISO, reaches supervised mksh,
  runs 68 userland utilities, has working signals/job control/time/ANSI
  terminal, and has a live virtio-net PCI device.
- The i486 lane is verified on `pc-i440fx-10.2` and `isapc` with `-cpu 486`.
- The x86_64 lane boots real Ring 3 PID 1 `/bin/sh` on the exact QEMU Q35 gate.
- The x86_64 grammar and token-recognition denominators are completely closed.
- `xorriso` is still a host prerequisite for ISO assembly.

## Not Verified Today

- lwIP TCP/IP stack integration (pending)
- Full socket semantics beyond the verified AF_INET loopback datagram scope,
  including TCP/IP, AF_UNIX, stream listen/accept, ancillary messages, and
  socket pairs (pending)
- Full dietlibc static linking from TCC; current verified TCC runtime is a
  small XINIM-native shim for simple C programs.
- pkgsrc bootstrap (pending)
- Full POSIX conformance is not claimed. The exact open rows are retained in
  `docs/posix/posix_shell_language_ledger.tsv`,
  `docs/posix/posix_issue7_utility_ledger.tsv`,
  `docs/posix/posix_system_prerequisites.tsv`, and the expanded Base
  Definitions/System Interfaces ledgers. Bounded Issue 7 closure requires
  245/245 parent rows plus all seven prerequisites; full SUSv4/POSIX closure
  additionally requires all 18,513 expanded rows.

## Architectural Truth

- Each build tree configures exactly one CPU lane.
- Conan manages host-side build integration only.
- The 32-bit lane uses GRUB plus Multiboot2.
- The x86_64 lane uses Limine.
- `mksh` is the active i486 guest shell baseline.
- `dietlibc` is the i486 userland C library (vendored, retargeted) for the
  normal utilities; TCC currently uses its own small runtime shim.
- ring3.cpp is the monolithic Ring 3 supervisor (~4040 lines).
