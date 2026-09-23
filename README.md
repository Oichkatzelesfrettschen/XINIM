# XINIM i486 Operating System

A bare-metal operating system for i486 and later 32-bit x86 hardware.
Boots from a single dynamic VMDK or qcow2 disk image with embedded GRUB, runs an
interactive mksh shell with 70+ POSIX utilities, pipes, signals,
file I/O, and a C compiler (TCC).

## Quick Start

### Requirements

- Linux host (tested on CachyOS/Arch)
- Clang 18+ and the matching target binutils package for optional cross-ELF builds
- CMake 3.28+, Ninja
- GRUB tools: `grub-mkimage` (package: `grub`)
- Filesystem tools: `mke2fs` (package: `e2fsprogs`)
- QEMU: `qemu-system-i386`, `qemu-img`
- VirtualBox (optional): `VBoxManage` for automated testing

### Build

```bash
# Configure (pure CMake, no Conan needed)
cmake --preset i486-standalone

# Build kernel + all utilities + bootable dynamic VMDK plus qcow2 disk
cmake --build build/i486/Debug --target i486_boot_disk

# CMOV-capable Pentium III / i686 lane
cmake --preset i686-standalone
cmake --build build/i686/Debug --target i686_boot_disk
```

### Launch

```bash
# QEMU (quick)
scripts/qemu_i486.sh

# VirtualBox (stricter hardware emulation)
scripts/test_i486_vbox.sh
```

### Interactive Use

```bash
# QEMU with VGA window
qemu-system-i386 -machine pc -cpu 486 -m 256M -boot c \
  -drive file=build/i486/Debug/images/i486/xinim-i486-boot.vmdk,format=vmdk \
  -vga std -serial stdio
```

Type commands in the VGA window. The shell prompt `$` accepts standard
Unix commands.

## What Works

### Shell and Builtins

- **mksh** interactive shell with job control
- `echo`, `pwd`, `cd`, `true`, `false`, `test`
- Shell pipes: `echo hello | cat`, `ls /bin | wc -c`
- File redirection: `echo data > /tmp/file`, `cat /tmp/file`

### POSIX Utilities (70+)

`awk` `basename` `cat` `chmod` `chown` `cksum` `clear` `cmp` `cp`
`cut` `date` `dd` `df` `diff` `dirname` `du` `echo` `env` `expand`
`expr` `false` `file` `find` `fold` `grep` `head` `hexdump`
`hostname` `id` `join` `kill` `ln` `logname` `ls` `md5sum` `mktemp`
`mv` `nice` `nl` `nohup` `nproc` `paste` `patch` `printenv`
`printf` `ps` `pwd` `readlink` `rm` `sed` `seq` `sleep` `sort`
`split` `strings` `tac` `tail` `tar` `tee` `test` `time` `touch`
`tr` `true` `tty` `uname` `uniq` `wc` `wget` `which` `whoami`
`xargs` `yes`

### Compiler Toolchain

- **TCC 0.9.27** -- Tiny C Compiler (210KB, compiles C on-target)
- **bmake** -- BSD make (201KB)
- **dietlibc headers** at `/usr/include`
- On-target compile/run smoke path:
  `tcc -static -Wl,-Ttext=0x00400000 -o /persist/a.out /persist/a.c`

### Kernel Features

- Per-process file descriptor table with refcounting
- Pipe blocking with event-driven EOF detection
- Signal delivery (SIGCHLD, SIGTTIN, SIGHUP, Ctrl+C, Ctrl+Z)
- 4 MB process address space (page-granularity GDT segments)
- ext2 filesystem with read/write/create/chmod support
- Device files: `/dev/tty`, `/dev/null`, `/dev/zero`, `/dev/console`
- VGA text mode with ANSI escape sequences and keyboard echo
- PS/2 keyboard input with full US layout
- COM1/COM2 serial I/O
- ATA disk driver (PIO mode)
- virtio-net driver (QEMU) / Am79C970A (VirtualBox)
- Controlling terminal with job control (SIGTTIN, SIGTTOU, SIGHUP)
- Supervised service restart with DAG dependency ordering

### Boot Architecture

- Single dynamic VMDK/qcow2 disk image with embedded GRUB (MBR + core.img)
- ext2 root partition with kernel, shell, and all utilities
- 7 Multiboot2 modules (kernel + xash + mksh + holdsvc + cat + ls + echo)
- Remaining 70+ utilities loaded from ext2 on demand via execve

## Build Targets

| Target | Description |
| -------- | ------------- |
| `xinim_i486` | Kernel binary |
| `i486_boot_disk` | VMDK and qcow2 boot disks with all utilities |
| `i686_boot_disk` | CMOV-capable Pentium III lane boot disks |
| `xinim_i486_image` | ISO + ATA disk images (legacy) |
| `dietlibc_i486` | dietlibc C library |
| `mksh_i486` | mksh shell |

## Testing

### Automated (VirtualBox)

```bash
scripts/test_i486_vbox.sh
```

Boots VBox headless, types commands via `VBoxManage keyboardputscancode`,
checks COM1 for faults. 37/37 tests pass with zero faults.

### Manual (QEMU)

```bash
scripts/qemu_i486.sh --boot-disk build/i486/Debug/images/i486/xinim-i486-boot.vmdk
```

### Automated (QEMU boot disk)

The disk regression uses QEMU's `pc-i440fx-11.1` machine, `486` CPU, TCG,
and the launcher's virtio network device. QEMU must provide that machine
version. The tests exercise VMDK at 64 and 256 MiB and qcow2 at 64 MiB,
plus ISO boot with ATA storage, with disposable disk writes and retained
serial transcripts. The ATA latency test limits reads to 16 KiB/s.

```bash
cmake --build build/i486/Debug \
  --target i486_boot_disk test_i486_dma_pages test_i486_scheduler_wait \
  test_i486_process_sessions test_i486_signal_dispatch \
  test_i486_context_switch test_i486_ata_deadline test_i486_ext2_indirect -j2
ctest --test-dir build/i486/Debug \
  -R '^(test_i486_|i486_.*disk_shell|i486_ata_latency_test)' \
  --output-on-failure
```

See [boot and input repair evidence](docs/analysis/I486_QEMU_BOOT_REPAIR.md)
for failure mechanisms and replay commands, and
[i486 memory and scheduler design](docs/analysis/I486_MEMORY_SCHEDULER_DESIGN.md)
for source-derived architecture constraints and bounded follow-up designs.

## Architecture

```text
XINIM i486 Architecture
=======================

User Space (Ring 3, 4 MB per process, max 8 processes)
  mksh shell <-> POSIX utilities <-> TCC compiler
       |              |                    |
  [syscall int 0x80]  |                    |
       |              |                    |
Kernel (Ring 0, flat 4 GB segments)
  +-- Syscall dispatcher (112 syscalls)
  +-- Process manager (fork, exec, wait, exit, signals)
  +-- Per-process fd table (fd_map[32] -> global OpenFile slots)
  +-- bootfs (RAM filesystem from Multiboot2 modules)
  +-- ext2 reader/writer (ATA PIO disk I/O)
  +-- Pipe subsystem (4 KB circular buffers, blocking, EOF events)
  +-- Console (VGA text mode + PS/2 keyboard + COM1/COM2 serial)
  +-- Timer (PIT 100 Hz, preemptive scheduling)
  +-- Signal delivery (trampoline on user stack, sigreturn)
  +-- Supervised service manager (DAG restart ordering)

Hardware
  +-- i486 CPU (no paging, GDT segment isolation)
  +-- VGA text mode at 0xB8000
  +-- PS/2 keyboard (port 0x60/0x64)
  +-- COM1 (0x3F8, kernel log) + COM2 (0x2F8, shell I/O)
  +-- ATA primary master (PIO, ext2 filesystem)
  +-- PIT timer (IRQ0, 100 Hz)
  +-- PIC (8259A, IRQ routing)
```

## Key Files

| File | Purpose |
| ------ | --------- |
| `src/kernel/i486/ring3.cpp` | Kernel core: processes, syscalls, signals |
| `src/kernel/i486/bootfs.cpp` | VFS: file I/O, pipes, devices |
| `src/kernel/i486/console.cpp` | VGA + keyboard + serial |
| `src/kernel/i486/ext2_reader.cpp` | ext2 filesystem driver |
| `src/kernel/i486/elf32_loader.cpp` | ELF binary loader |
| `scripts/create_i486_boot_disk.py` | Raw, qcow2, and VMDK image builder |
| `scripts/qemu_i486.sh` | QEMU launcher |
| `scripts/test_i486_vbox.sh` | VirtualBox automated test suite |
| `libc/dietlibc-xinim/` | Modified dietlibc for XINIM syscalls |

## License

BSD 3-Clause. See [LICENSE](LICENSE).
