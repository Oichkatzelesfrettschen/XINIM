# XINIM 32-bit QEMU Debugging

Official QEMU documentation for this lane is cached and indexed in
`docs/external_sources/QEMU_X86_PC_SOURCES.md`; use that source index as the
hardware contract before changing QEMU machine, CPU, disk, network, or GDB
debugging assumptions.

For the full RCA and host-tool instrumentation map, see
`docs/testing/X86_32_RCA_INSTRUMENTATION.md`.

This lane should be debugged in buckets, not by line-by-line boot poking.
Start with the instrumented runner:

```sh
python3 scripts/qemu_x86_32_debug.py --build-dir build/i686/Debug --lane i686 --boot-mode disk --scan-local-repos
python3 scripts/qemu_x86_32_debug.py --build-dir build/i686/Debug --lane i686 --boot-mode iso
```

Outputs land under `build/i686/Debug/logs/qemu-debug/<lane>/<boot-mode>/`:

- `serial.log`: guest serial markers from the kernel and shell lane.
- `qemu-trace.log`: QEMU `-d` trace with CPU exceptions, reset, unimplemented, and guest errors.
- `qemu-device-help.txt`: QEMU `pc` device inventory for driver matching.
- `summary.md`: classified fault bucket and first missing boot marker.
- `local-driver-source-scan.md`: optional local-only provenance map, written under
  `build/` and ignored by git.

Use `--trace-profile exec` when a CPU exception needs instruction context, and
`--gdb` to stop at reset with QEMU's default GDB stub on `tcp::1234`.

## Fault Buckets

- Build/config: target does not build, host flags leak into the freestanding lane, or required tools are missing.
- Image/container: wrong disk format, corrupt MBR, missing partition, bad ext2, or `qemu-img check` failure.
- Bootloader/handoff: no `XINIM <lane> Booting` banner, no `boot protocol: multiboot2`, or missing modules.
- CPU exception: invalid opcode, page fault, general protection fault, double fault, or triple fault in `qemu-trace.log`.
- Device/driver: no ATA detection, no PCI enumeration, no virtio/e1000 device match, wrong QEMU machine/device model.
- Filesystem: ext2 superblock marker missing, mount registration missing, or path probes missing.
- Userspace: scheduler marker present but no shell prompt, command response, or persistence behavior.

## QEMU Driver Inventory

For the current `pc` machine, the boot path depends on these device classes:

- BIOS/GRUB: i386-pc BIOS boot, MBR, ext2, Multiboot2 module loading.
- CPU: `486`, `pentium`, or `pentium3` per lane; i686 uses CMOV-capable `pentium3`.
- Serial: ISA 16550 on COM1 for kernel log and COM2 for shell tests.
- Block: legacy IDE/ATA primary master via QEMU's PIIX-style `pc` machine.
- PCI: PCI bus enumeration, with virtio-net optional and non-fatal when absent.
- Display: VGA text framebuffer, normally `-vga std`.

The CMOV-capable 32-bit lane is `i686` with QEMU CPU `pentium3`. The `i586`
lane uses QEMU CPU `pentium`, which keeps it useful as a Pentium baseline but
not as the CMOV target.

Reverse-engineer drivers by matching QEMU-visible hardware first:

```sh
qemu-system-i386 -machine pc -device help > build/i686/Debug/logs/qemu-device-help.txt
qemu-system-i386 -machine pc -cpu help > build/i686/Debug/logs/qemu-cpu-help.txt
```

Then build a small probe for each driver: enumerate, identify registers, perform
one read/write transaction, and print a single stable serial marker. Keep those
probes independent so a PCI bug does not hide an ATA bug.

## Fuzzers And Stressors

- Disk image fuzzing: mutate MBR partition entries, ext2 superblock fields, directory records, file sizes, and sparse writes; expected result is a clean error marker, never a CPU exception.
- Boot module fuzzing: vary module order, duplicate module names, empty module payloads, long module strings, and missing shell/hold service.
- Syscall fuzzing: run guest utilities against invalid pointers, bad fds, truncated paths, long paths, and unusual open flags.
- Shell/TTY fuzzing: random command fragments over COM2, CR/LF variants, backspace/control bytes, and prompt recovery.
- Driver fuzzing: QEMU device permutations such as no disk, empty disk, qcow2/vmdk/raw, extra PCI devices, virtio-net present/absent, and RAM size boundaries.
- Exception fuzzing: QEMU `-d int,cpu_reset,guest_errors,unimp` plus GDB breakpoints on `panic`, fault stubs, and syscall entry.

Prefer libFuzzer/AFL++ for host-side parsers such as MBR, ext2, Multiboot2
tags, and ELF32 loading. Use QEMU integration fuzzing for hardware state and
userspace syscall behavior.

## Local Reference Mining

XINIM is BSD-3-Clause-derived, so direct adaptation should prefer BSD, MIT, ISC,
public-domain, or clearly compatible code. GPL code can be useful for behavior
comparison and tests, but should not be copied into BSD-licensed kernel files.
Old UNIX/Solaris-like drops need explicit license review before reuse.

Keep personal repo names, journal paths, and project notes out of tracked docs.
The scanner writes its findings into ignored build logs so local provenance can
guide the session without becoming repository content.

Source families worth scanning, when present locally:

- xv6-style MIT teaching kernels for simple IDE, UART, PIC, trap, and APIC models.
- CSRG/BSD-derived trees for permissively licensed Unix behavior and driver structure.
- Local experimental kernels for comparison only until their license is verified.
- Ancient UNIX/Solaris-like drops for archaeology only unless their redistribution
  and derivative-work status is explicit.

Use the scanner:

```sh
python3 scripts/qemu_x86_32_debug.py --build-dir build/i686/Debug --lane i686 --scan-local-repos
```
