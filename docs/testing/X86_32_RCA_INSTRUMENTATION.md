# XINIM 32-bit RCA And Instrumentation Matrix

Date: 2026-05-13
Scope: 32-bit XINIM QEMU lanes, especially the dynamic VMDK/qcow2 i686 path and
the compatibility i486/i586 paths.

This page is the operational map for debugging whole fault classes instead of
chasing one visible symptom at a time. It pairs each current RCA finding with
the tools that should catch the next instance.

## Current RCA Findings

| Issue | Root cause | Fix or guard |
| --- | --- | --- |
| Stale i586 build state | Old in-tree CMake/Conan cache carried host flags and stale toolchain state into the 32-bit lane. | Reconfigure clean out-of-tree build trees before treating compiler errors as source truth. |
| i386 namespace collision | `namespace xinim::userland::i386` collides with target macros on 32-bit Clang. | Keep ABI namespace names away from compiler predefined macro names. |
| Warning demotion | `-Wconversion` and `-Wsign-conversion` were demoted even under `XINIM_ENABLE_WERROR=ON`. | Treat them as errors; fix sign conversions explicitly. |
| Non-APIC boot hang | Legacy PIC/PIT setup was followed by the APIC masking path when APIC was disabled. | Keep non-APIC PIC/PIT initialization unmasked. |
| TTY desync | Console output drained input on every output character, which recursively perturbed COM2 tests. | Do not drain input from the output path; reserve tracing for targeted diagnostics. |
| Missing guest utilities | Disk-backed tests reached a shell but the ATA ext2 image lacked the real `/bin` utilities. | Copy the built XINIM utilities into ATA ext2 and generated boot disks. |
| i486 32M no-banner boot | The current bootfs/userland payload no longer reliably boots in the i486 lane at 32M. | Set the i486 QEMU lane memory to 48M; keep lower-memory work as a separate VFS-residency target. |
| `isapc` VGA failure | QEMU `isapc` with `-vga std` tried to open `vgabios.bin`, which was absent on this host. | Run serial-only `isapc` smoke with `XINIM_QEMU_VGA=none`. |
| Enhanced test false failures | Tests used `/tmp` for ext2 features, expected permissions from `ls -l` that were not printed, and treated optional TCC as mandatory. | Use `/persist`, print real mode strings in `ls -l`, and probe `/bin/tcc` before running TCC checks. |
| `chmod` false success | The syscall returned success even when no ext2/runtime chmod path applied. | Return `ENOENT` when no backed path exists or chmod cannot change the target. |
| COM2 echoed-command false failures | The shell/persistence harness used a 5-second command timeout; after long mksh/ext2 sequences, the guest answered later and the harness returned only the echoed command line. | Use a 30-second command timeout plus 1.5-second prompt settle for 32-bit QEMU shell tests. |
| CTest killed valid i686 shell run | The broadened kshell matrix completed successfully when run directly, but exceeded CTest's old 120-second outer timeout on this host. | Raise 32-bit shell/ext2 CTest budgets to 240 seconds and persistence to 180 seconds, keeping inner command timeouts bounded for real hangs. |
| `isapc` multi-boot prompt race | The ext2 mutation harness rebooted QEMU instances after only 0.5 seconds; the legacy `isapc` model could connect then close before the guest prompt was observed. | Use a 2-second QEMU reboot settle and a 40-second boot prompt window for the multi-boot ext2 mutation test. |
| TCP serial prompt loss | QEMU TCP serial can miss the first prompt if the guest prints before the client is attached, especially in multi-boot harnesses. | Send a blank line to redraw, then treat the explicit ready-marker handshake as authoritative. |
| Parallel CTest flakiness | Parallel QEMU tests reused shared disk artifacts and ports, causing locks and timing cross-talk. | Run full QEMU lane gates serially, or give every parallel test its own copied disk and port allocation. |
| Persistence harness false status | The persistence harness appended `; echo $?` inside commands that were already wrapped by a marker-status echo, so failures could be hidden by the status of the extra `echo`; the slow `isapc` path also exposed basename lookup fragility for disk helper binaries. | Parse the wrapper marker status directly and invoke disk helper utilities through absolute `/bin/...` paths. |
| TCC absent from images | CMake had helper scripts but no real TCC/bmake build targets; disk builders only accepted optional placeholder paths. | Build TCC/bmake as lane targets and stage them as required guest binaries when enabled. |
| TCC runtime not visible | The ext2 runtime mapper exposed only `/bin` and `/etc`, while TCC searches `/usr/include`, `/usr/lib`, and `/usr/lib/tcc`. | Expose `/usr` as a canonical ext2-backed prefix. |
| ext2 file sizes inflated | The ext2 writer copied the low 32-bit file size into `i_dir_acl`; for regular files this is the high 32 bits of size. | Keep `i_dir_acl=0` for regular-file write/truncate paths. |
| TCC executable refused | The ext2 executable loader capped files at 64 KiB, smaller than `/bin/tcc` and `/bin/bmake`. | Raise the loader buffer to 1 MiB. |
| TCC allocator failure | dietlibc malloc uses mmap/mremap-backed blocks; the kernel had too few user mapping slots and no `mremap`. | Increase mapping slots and implement minimal moving `mremap` for `MREMAP_MAYMOVE`. |
| TCC-linked binary outside user window | TCC defaults i386 executables to `0x08048000`, outside XINIM's segmented user window. | Compile in-guest programs with `-Wl,-Ttext=0x00400000`; keep this in the enhanced gate. |
| TCC crt syscall mismatch | A first-pass tiny crt used Linux `exit=1`, but XINIM uses `SYS_exit=25`. | Generate a XINIM-native TCC `crt1.o` using syscall 25. |
| i686 stricter shell harness hang | mksh reached controlling-TTY setup and called `fcntl(fd, F_DUPFD, FDBASE)`, but the ring3 fd table had been corrupted to map high descriptors to stdin. The corruption came from the 8 KiB per-process kernel stack growing downward into `context/mappings/fd_flags/fd_map` during syscall-heavy mksh startup. | Raise the ring3 kernel stack to 32 KiB, reset service fd maps to the canonical 0/1/2 console set before launch, and implement `F_DUPFD`/`F_DUPFD_CLOEXEC` allocation at or above the requested descriptor. |

## Modern TTY Patterns Borrowed Cleanly

The 2026-05-13 TTY/fd RCA used permissive references for behavior, not copied
implementation:

- SerenityOS (`BSD-2-Clause`) keeps init process stdio as three descriptors
  backed by the selected TTY and implements `F_DUPFD` by allocating at the
  caller-provided minimum descriptor.
- FreeBSD's current tty line discipline (`BSD-2-Clause`) separates canonical
  input availability, EOF/newline break handling, and output post-processing
  from low-level device emission.

The XINIM translation is C++-local and smaller:

- `reset_fd_map_to_console()` normalizes launched services to descriptors
  0, 1, and 2 only, matching the expected Unix process contract.
- `allocate_fd_map_entry_at_or_above()` gives `fcntl(F_DUPFD, min)` the
  descriptor-floor semantics that mksh expects for `FDBASE`.
- The line-discipline queues stay in `tty.cpp`; COM1/COM2 device emission stays
  under `console.cpp`; fd ownership remains in `bootfs.cpp` and per-process
  `fd_map`.

## Fault Classes And Instruments

| Fault class | Primary tools | What to collect |
| --- | --- | --- |
| Configure/build drift | `cmake`, `ninja`, `clang`, `clang-tidy`, `scan-build`, `intercept-build` | CMake cache, compile commands, exact lane flags, warnings-as-errors output. |
| Static C/C++ defects | `semgrep`, `cppcheck`, `clang-tidy`, `scan-build` | Nullability, bounds, unchecked return values, conversions, portability issues. |
| Shell/script defects | `shellcheck`, `shfmt`, `python -m py_compile` | Syntax, quoting, subprocess, path, and harness errors before QEMU starts. |
| ELF/layout defects | `llvm-readelf`, `llvm-objdump`, `llvm-nm`, `llvm-dwarfdump`, `pahole`, `scanelf`, `checksec` | Entry points, sections, symbols, relocations, DWARF layout, hardening properties for hosted tools. |
| Reverse engineering | `gdb`, `radare2`, `rizin`, `ghidra`, `retdec`, `binwalk`, `python-capstone`, `python-keystone` | Disassembly, decompilation, byte-pattern checks, instruction encode/decode experiments. |
| QEMU exception/debug | `scripts/qemu_x86_32_debug.py`, QEMU `-d`, QEMU `-D`, QEMU `-s -S`, `gdb` | Serial markers, `int/cpu_reset/guest_errors/unimp` trace, registers, fault vectors, stop-at-reset snapshots. |
| Disk image defects | `qemu-img`, `qemu-io`, `qemu-nbd`, `fsck.ext2`, `debugfs`, `guestfish`, `guestmount`, `virt-inspector` | VMDK/qcow2/raw format, MBR, partitions, ext2 superblock, `/bin` payload, `/persist` mutation state. |
| Host runtime effects | `strace`, `ltrace`, `perf`, `bpftrace`, `valgrind`, `rr` | QEMU host file/device access, image locks, process exits, host-only reproducer memory bugs. |
| Coverage | `kcov`, `lcov`, `gcovr`, Clang SanitizerCoverage for host harnesses | Host parser/test coverage and fuzzer corpus impact. |
| Fuzzing | AFL++, `honggfuzz`, libFuzzer, `radamsa`, `afl-utils` | Crash corpus, minimized inputs, coverage growth, regression seeds. |
| Linux reference stress | `syzkaller`, `trinity` | Reference syscall patterns only; use as inspiration until XINIM has a syscall executor/harness. |
| Alternate emulator | `bochs` when available | Cross-check CPU/legacy-device assumptions outside QEMU. |

## Fuzzer Targets

Start with host-side parsers and pure functions, where sanitizers and coverage
are strongest:

- MBR and partition table parsing.
- ext2 superblock, group descriptor, inode, directory, sparse write, and path
  traversal logic.
- Multiboot2 tag parsing and module ordering.
- ELF32 loader metadata and symbol/path handling.
- Shell lexer/parser command fragments, CR/LF handling, and control bytes.

Then use QEMU integration fuzzing for whole-system state:

- RAM sizes around known boundaries: 32M, 40M, 48M, 64M.
- Disk formats and geometry: raw, qcow2, dynamic VMDK, missing disk, empty disk,
  malformed ext2, read-only disk.
- Device permutations: `pc`, `isapc`, VGA off/on, extra PCI devices, virtio-net
  present/absent, IDE present/absent.
- COM2 command fuzzing with prompt recovery and no-kernel-panic invariants.

Use libFuzzer for in-process host harnesses; LLVM documents it as a
coverage-guided engine linked into the target with SanitizerCoverage. AFL++ is
the campaign manager for source-available and binary-only modes, while QEMU's
own device-fuzzing documentation is relevant if the work pivots to fuzzing QEMU
devices themselves rather than the XINIM guest.

## QEMU Driver Plan

The QEMU-side contract is documented and cached through
`docs/external_sources/QEMU_X86_PC_SOURCES.md`. The active bring-up machine is
the QEMU `pc` family, with legacy devices first:

1. CPU lane: `i486` maps to `486`, `i586` maps to `pentium`, and CMOV-capable
   `i686` maps to `pentium3`.
2. Serial: COM1 is kernel logging, COM2 is shell harness I/O.
3. Interrupts/timer: PIC/PIT is the baseline; APIC is optional and must not mask
   the legacy lane.
4. Block: IDE/ATA primary master is the boot/persist disk path.
5. Filesystem: MBR plus ext2 is the verified mutation surface.
6. PCI: enumerate before binding optional NICs.
7. Network: stage from PCI enumeration, to emulated NIC probe, to packet I/O;
   use virtio-net only after virtqueue/DMA support exists.

Reverse engineering should start from QEMU-visible hardware inventory:

```sh
qemu-system-i386 -machine pc -device help
qemu-system-i386 -machine pc -cpu help
python3 scripts/qemu_x86_32_debug.py --lane i686 --boot-mode disk --trace-profile crash
python3 scripts/qemu_x86_32_debug.py --lane i686 --boot-mode disk --trace-profile exec --gdb
```

Keep local repo scans disposable and ignored. Tracked docs can say which source
families are license-compatible; raw personal repo paths and journal notes stay
out of git.

## Host Package State

Configured package repos, including BlackArch, CachyOS, and Arch repos, are
first-class sources on this host. Treat installed BlackArch packages such as
angr, RetDec, Trinity, and syzkaller as normal RCA/debug tooling, while still
checking each tool's license before copying code or data into the repository.

Installed via `paru` on 2026-05-13 for this RCA pass:

- `libguestfs`: `guestfish` and `guestmount` for image inspection.
- `guestfs-tools`: `virt-inspector` and higher-level guest image helpers.
- `ltrace`: host dynamic-library tracing.
- `pax-utils`: `scanelf`, `dumpelf`, `lddtree`.
- `checksec`: hosted ELF hardening checks.
- `qemu-user`: `qemu-i386` for user-mode i386 checks.
- `keystone` and `python-keystone`: instruction assembly experiments.
- `afl-utils`: AFL campaign and crash-corpus helpers.
- `radamsa`: mutation fuzz input generation.
- `bochs`: alternate x86 emulator for CPU and legacy-device cross-checks.
- `nbdkit`: scriptable NBD exports for block-image fault injection and
  host-side disk experiments. Its AUR `check()` failed two libguestfs-backed
  FAT/VDDK tests on this host, so it was installed with `--nocheck` after the
  compiled tool and `nbdkit --dump-config` verified locally.
- `genext2fs`: deterministic ext2 image construction for fixture generation.
- `e2tools`: direct ext2 file injection and inspection helpers such as `e2cp`
  and `e2ls`.

Already present before that install:

- QEMU system/image tools, Clang/LLVM/LLD, GDB, rr, AFL++, honggfuzz, radare2,
  rizin, Ghidra, RetDec, binwalk, strace, valgrind, bpftrace, perf, semgrep,
  cppcheck, shellcheck, shfmt, kcov/lcov/gcovr, syzkaller, trinity, Capstone,
  Unicorn, angr, user-local Frida tools, e2fsprogs, mtools, xorriso, and GRUB.

The host audit should now report zero missing tools. Keep these as expanded RCA
tools rather than minimal build requirements: the normal build needs only the
core compiler, image, and QEMU tools, while this set is for debugging,
instrumentation, fuzzing, reverse engineering, and disk forensics.

## Tool Package Use Cases

Use this package matrix to pick a tool by fault shape before dropping into a
line-by-line debug loop.

| Package or tool family | XINIM use case | Good next action |
| --- | --- | --- |
| CMake, Ninja | Detect stale configure state and lane/property drift. | Reconfigure cleanly, then build the exact lane target. |
| Clang, LLVM, LLD, libc++ | Build 32-bit freestanding and hosted pieces with warnings-as-errors. | Inspect `compile_commands.json`, lane flags, and linker maps. |
| `clang-tidy`, `scan-build`, `intercept-build` | Catch static C/C++ defects before booting QEMU. | Capture the compile database and run targeted analyzer passes. |
| `semgrep`, `cppcheck` | Scan for semantic bugs, unchecked errors, and portability traps. | Add a narrow rule or suppress only with a cited reason. |
| `shellcheck`, `shfmt`, `python -m py_compile` | Validate shell and Python harnesses before blaming the guest. | Run the syntax stage from `scripts/x86_32_full_gate.sh`. |
| `llvm-objdump`, `llvm-readelf`, `llvm-nm` | Verify ELF32 layout, entry points, sections, symbols, and relocations. | Compare kernel, userland, and TCC-linked guest binaries. |
| `llvm-dwarfdump`, `pahole` | Inspect debug info and struct layout for ABI or stack corruption. | Check PCB, fd table, syscall frame, and ext2 structure sizes. |
| `scanelf`, `dumpelf`, `checksec` | Inspect hosted binary metadata and hardening. | Use on host tools and bootstrapped compiler artifacts. |
| `gdb`, `rr` | Debug host repros and QEMU GDB-stub stops. | Use `--gdb` in the QEMU debug helper for guest reset/fault stops. |
| `strace`, `ltrace` | Prove host-side file, socket, and library-call behavior. | Trace QEMU or helper tools when image locks or missing files are suspected. |
| `perf`, `bpftrace`, `valgrind` | Profile host hot loops and memory bugs in host harnesses. | Use on parsers, image builders, or QEMU-host repros, not guest kernel code directly. |
| `qemu-system-i386`, `qemu-i386` | Run full-system lanes and user-mode i386 smoke checks. | Prefer full-system QEMU for kernel/device RCA; use user-mode checks for hosted utilities. |
| `qemu-img`, `qemu-io`, `qemu-nbd` | Validate, probe, and export raw/qcow2/VMDK disk images. | Run `qemu-img check`; use `qemu-io` or NBD for block-level repros. |
| `fsck.ext2`, `debugfs`, `e2tools` | Inspect and manipulate ext2 images without booting XINIM. | Confirm superblocks, directory entries, and staged `/bin` payloads. |
| `guestfish`, `guestmount`, `virt-inspector` | Mount or inspect guest images through libguestfs. | Cross-check image contents when the guest sees different disk state. |
| `genext2fs` | Build deterministic ext2 fixtures. | Recreate small ext2 cases for parser and mutation tests. |
| `mtools`, `xorriso`, GRUB | Build and inspect boot media. | Check ISO/FAT/GRUB generation before kernel handoff RCA. |
| AFL++, `afl-utils`, `honggfuzz`, `radamsa` | Fuzz host parsers and mutate command/disk inputs. | Start with MBR, ext2, Multiboot2, ELF32, and shell lexer harnesses. |
| `syzkaller`, `trinity` | Reference syscall stress patterns. | Mine patterns and invariants; do not treat Linux behavior as XINIM ABI. |
| `radare2`, `rizin`, Ghidra, RetDec, Binwalk | Reverse engineer binary behavior and containers. | Use for local artifacts, permissively licensed references, and generated guest binaries. |
| Capstone, Keystone, Unicorn, angr, Frida | Instruction decode/assemble, emulation, symbolic, and dynamic analysis. | Prototype instruction/syscall edge cases outside the guest first. |
| `bochs` | Independent x86 emulator cross-check. | Reproduce CPU or legacy-device assumptions when QEMU behavior is suspect. |
| `nbdkit` | Scriptable disk backends and image fault injection. | Export a controlled NBD disk to reproduce block-layer edge cases. |

Run the local audit:

```sh
bash scripts/x86_32_tooling_audit.sh
```

Run the serial full gate:

```sh
bash scripts/x86_32_full_gate.sh
```

## Gate Order

Use this order for a maximal but bounded RCA pass:

1. `bash scripts/x86_32_tooling_audit.sh`
2. `python3 -m py_compile scripts/qemu_x86_32_debug.py test/boot/x86_32_*.py`
3. `bash -n scripts/build.sh scripts/build_i486.sh scripts/qemu_i486.sh`
4. `cmake --build build/i686/Debug --target all`
5. Serial lane gates, not parallel, for `i686`, `i586`, and `i486` with
   `ctest --test-dir <build-dir> --output-on-failure -E 'vbox'`
6. `qemu-img check` for generated VMDK/qcow2 artifacts.
7. Targeted debug rerun with `scripts/qemu_x86_32_debug.py` for any missing
   marker, exception, or image fault.

`scripts/x86_32_full_gate.sh` runs the same sequence as a single local command.

## Verification Snapshot

Additional targeted checkpoint on 2026-05-13:

- `ctest --test-dir build/i486/Debug -R '^i486_enhanced_test$'
  --output-on-failure` passed after requiring `/bin/tcc`.
- The enhanced gate now compiles and runs a C program inside XINIM using
  `tcc -static -Wl,-Ttext=0x00400000`.
- `qemu-img check` found no errors in the regenerated i486 dynamic VMDK and
  qcow2 boot images.

On 2026-05-13, a pre-TCC checkpoint of `bash scripts/x86_32_full_gate.sh`
completed successfully in a `tmux` session on this host:

- `i686`: 8/8 CTest integration tests passed.
- `i586`: 8/8 CTest integration tests passed.
- `i486`: 12/12 CTest integration tests passed, including `isapc` smoke,
  shell, persistence, and ext2 mutation coverage.
- `qemu-img check` found no errors in generated `i486`, `i586`, and `i686`
  dynamic VMDK and qcow2 boot images.
- `git diff --check` passed.

The same run confirmed that `i686_kshell_test` and `i586_kshell_test` can take
about 133 seconds on this machine, and `i486_isapc_ext2_mutation_test` can take
about 191 seconds. Keep the CTest outer timeouts above those observed values so
valid slow boots are not killed while the inner command timeouts still catch
actual guest hangs.

Intermediate 2026-05-13 checkpoint before the later i686 fd/stack harness fix:

- `i486_enhanced_test` passes with required in-guest TCC, including compile and
  execution of a simple `/persist` program.
- `qemu-img check` passes for generated i486 dynamic VMDK/qcow2 artifacts.
- `i686_kshell_test` was not green under the stricter harness at that moment;
  it reached supervised ring3 launch and then timed out before a mksh prompt.

Final 2026-05-13 checkpoint after the optional disk/debug tool install:

- `bash scripts/x86_32_tooling_audit.sh` reports zero missing tools, including
  `bochs`, `nbdkit`, `genext2fs`, and `e2tools`.
- `bash scripts/x86_32_full_gate.sh` passed in a `tmux` session.
- `i686`: 8/8 CTest integration tests passed, including kshell, persistence,
  ext2 mutation, and enhanced TCC coverage.
- `i586`: 8/8 CTest integration tests passed.
- `i486`: 12/12 CTest integration tests passed, including `isapc` smoke,
  shell, persistence, and ext2 mutation coverage.
- `qemu-img check` found no errors in generated `i486`, `i586`, and `i686`
  dynamic VMDK and qcow2 boot images.
- `git diff --check` passed.
