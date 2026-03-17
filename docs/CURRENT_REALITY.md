# Current Reality

Date: 2026-03-17

This file is the short checkpoint for what the repository actually does today.

## i486 Lane -- Full-System Bring-Up Status

The i486 lane has reached **Phase 3 completion** with Phase 4 networking
infrastructure in place.

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
- 16 process slots, 64 MB QEMU RAM
- 32 fd table, 256-byte paths, 16 pipes at 4096 bytes
- O_NONBLOCK enforcement on pipes (-EAGAIN/-EPIPE)
- Proper errno returns throughout (ENOENT, EBADF, EFAULT, ECHILD, EINTR, etc.)
- ANSI CSI escape sequence parsing (colors, cursor, clear screen)
- Termios ioctls (TCGETS/TCSETS, TIOCGWINSZ, TIOCGPGRP)
- ext2 timestamps (atime/ctime/mtime) on create and write
- execve resets signal handlers, closes FD_CLOEXEC descriptors
- Orphan reparenting to PID 1

### Networking

- **DMA page allocator**: bump allocator over Multiboot2 free memory above 4MB
- **virtio-net driver**: PCI discovery, feature negotiation, MAC address,
  DMA-backed virtqueue ring allocation, TX/RX paths, DRIVER_OK set
- **PCI subsystem**: initialized in i486 main.cpp
- **QEMU**: virtio-net-pci device attached
- **Socket syscalls**: 15 numbers reserved (81-95), returning -ENOSYS
- **Next**: lwIP integration for TCP/IP

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

### Build System

- 76 GRUB module2 entries for bootfs
- Loop-based CMake build for all dietlibc-linked utilities
- ext2 ATA disk with /etc/profile, /etc/mkshrc
- mksh rebuilt with HAVE_SELECT, HAVE_GETSID, HAVE_FTRUNCATE, HAVE_SIG_T,
  HAVE_FLOCK enabled

## x86_64 Lane

- GRUB/Multiboot2 ISO reaches staged shell over COM2
- Shell smoke test validates basic commands
- Limine bootstrap for x86_64 image path

## Verified Today

- The canonical build flow is pure Conan + CMake rooted in
  `build/<lane>/<config>`.
- `conan install` plus `cmake --preset ...` works for all lanes.
- The i486 lane produces a bootable GRUB ISO, reaches supervised mksh,
  runs 68 userland utilities, has working signals/job control/time/ANSI
  terminal, and has a live virtio-net PCI device.
- The i486 lane is verified on `pc-i440fx-10.2` and `isapc` with `-cpu 486`.
- `xorriso` is still a host prerequisite for ISO assembly.

## Not Verified Today

- lwIP TCP/IP stack integration (pending)
- Socket syscalls beyond -ENOSYS stubs (pending)
- TCC compiler running inside XINIM (pending)
- pkgsrc bootstrap (pending)
- Full POSIX compliance (partial -- core subset working)

## Architectural Truth

- Each build tree configures exactly one CPU lane.
- Conan manages host-side build integration only.
- The 32-bit lane uses GRUB plus Multiboot2.
- The x86_64 lane uses Limine.
- `mksh` is the active i486 guest shell baseline.
- `dietlibc` is the i486 userland C library (vendored, retargeted).
- ring3.cpp is the monolithic Ring 3 supervisor (~4040 lines).
