# i486 QEMU boot, input, and memory ownership

The README boot-disk command failed on a fresh checkout because upstream
moved the pinned bmake archive. Once the disk existed, immediate shell
input exposed a scheduler continuation defect. A separate DMA ownership
error allowed the launcher's virtio device to overlap process storage.

## Reproduced failures and repairs

`scripts/build_i486_bmake.py` requested `bmake-20240808.tar.gz` from the
upstream release directory and received HTTP 404. The upstream `old/`
directory serves identical pinned bytes, SHA-256
`b59189251b483decd4492f1f74387b2a584c03d5aa4637cd48b38ec62b9c0848`.
The recipe retains the version, digest, compiler, and target profile.

`sched.cpp::block_current_process_until_rescheduled` marked a reader
waiting, polled readiness, and selected a runnable process. Input arriving
during that poll could reselect the caller. The unconditional switch then
restored the caller's syscall-entry registers while its read still owned
the active kernel stack. The syscall returned without completing the
buffer/result contract. Immediate COM2 input reproduced `rintf` in place
of `printf`; an instrumented run recorded:

```text
scheduler probe: wait selected current pid=1 eax=5
i486 fault vector=6 error=0x00000000 pid=1 eip=0x00000003
Respawning supervised service init-shell
```

`SYS_read` is 5 in the XINIM ABI. The repair switches contexts only when
the selected process differs from the caller. An immediate wake completes
the existing syscall, including continuation cleanup and signal handling.
The native test compiles the real scheduler and supplies device/assembly
seams; the old path fails its zero-switch assertion. Peer dispatch remains
covered. The guest regression sends input immediately after each prompt,
without the older shell harness's 1.5-second prompt-settling delay.

Peer selection also needs the target's continuation state. A process woken
inside a blocking syscall must resume its saved kernel stack before
returning to Ring 3. The shared switch now saves the outgoing continuation
and selects either the incoming kernel stack or the incoming user context.
Child waits use the scheduler's child-state wakeups, preserving stopped
children and pending syscall completion.

`dma_pages.cpp` assumed everything below 4 MiB belonged to the kernel and
everything above could supply DMA. The baseline ELF instead placed
`g_processes` at `0x003da1c0`, with size `0x02049600`; the kernel load ended
at `0x02481f40`. DMA at `0x00400000` therefore overlapped the first process.
The baseline could reach a shell despite the overlap. The overlap is an
ownership defect; the retained trace attributes the observed input fault
to the scheduler's same-process switch.

The allocator now subtracts the linker-defined kernel span, original
Multiboot information bytes, retained modules, and reserved firmware
ranges before selecting one contiguous page-aligned pool. Allocation
checks alignment and arithmetic bounds, and initialization discards stale
state. The scan uses repeated bounded walks over the boot description
instead of another fragment array. Native tests cover actual zeroing,
occupied-byte preservation, fragmentation, invalid inputs, exhaustion,
and reset. Removing kernel exclusion makes the regression fail.

An ordinary child exit also exposed a terminal-lifetime defect. The parent
copied `ctty_slot` through fork, while the exit path treated any nonnegative
slot as session leadership. The command below returned 129 before its
subshell could print the expected line:

```sh
(/bin/hello child >/dev/null && printf 'subshell-alive\n')
```

Process state now distinguishes session identity, process group, and
controlling-terminal association. Console ownership belongs to a session;
closing or recycling a descriptor cannot confer leadership. Fork inherits
membership, while setsid and supervised reload create new membership.
The supervisor retains public PID 1, so an internal generation separates
its fresh session from surviving members of an earlier session. Foreground
group ownership continues to use the existing bootfs storage.

Terminal acquisition and group-changing ioctls check that ownership.
Leader departure signals the owning session's foreground group and
detaches its members. Ordinary child departure releases only the child's
association. Signal termination uses the normal exit lifecycle for
descriptor release, parent notification, and service restart. Stopped
processes leave user dispatch; SIGCONT makes them eligible again.
Pending deliverable signals cross the syscall-return boundary before the
caller resumes user instructions, including a shell sending SIGKILL to itself.
The disk regression checks both subshell survival and a supervised shell
restart after an explicit `kill -KILL $$`.

The wider shell gate exposed an intermittent executable lookup failure.
A controlled ISO boot with the ATA data disk limited to 16,384 read bytes
per second reproduced a lower-layer failure:

```text
ATA failure stage=busy-timeout lba=0x00000804 status=0x000000D0
ext2 reader: superblock load failed
```

The original uninstrumented exec failure identifies the symptom. The delayed
disk probe independently establishes the ATA failure mechanism.

The IDE driver bounded completion by 100,000 status reads. QEMU's
`hw/ide/core.c::ide_sector_read` sets BUSY until asynchronous block I/O
completes; instruction count cannot measure the storage deadline.
The repair measures a three-second accumulated PIT channel 2 budget while
interrupts remain masked. PIT channel 0 retains scheduler ownership, and
the clock works on the ISA lane without a TSC or ACPI dependency. Status
readiness wins before deadline expiry. A timed-out outstanding command
retires the device and its storage callbacks until reboot, preventing a
later transfer from consuming stale PIO data. Successful completion also
requires BUSY and DRQ to clear.

The 16-bit PIT counter wraps every 54.9 ms. Polling accumulates modular
deltas; whole wraps missed during longer host stalls conservatively extend
the budget. The budget measures observed hardware ticks rather than an
absolute host-wall deadline. `XINIM_X86_32_EXEC_IO_TRACE=ON` enables
failure-only ATA, ext2, and exec diagnostics without changing timeout policy.

The enhanced guest compiler test identified two further integration errors.
TinyCC searched `/usr/lib/tcc` for startup objects staged in `/usr/lib`,
and its recipe generated a substitute startup/libc despite receiving the
configured dietlibc inputs. TinyCC now uses those dietlibc bytes and their
argc/argv/environment ABI. The owned math adapter builds as C++23; the
pinned compiler source retains its upstream language and provenance.

The real dietlibc archive is 896,046 bytes. The ext2 reader previously
stopped after twelve direct blocks and one single-indirect block: 274,432
bytes with 1 KiB blocks. TinyCC consequently rejected valid object members
above that boundary. The reader now traverses double and triple indirection
using the existing scratch buffer. Storage mutation remains admitted only
within the direct/single-indirect implementation. Unsupported writes,
truncation, removal, and replacement fail before changing data or metadata;
safe metadata-only operations retain their existing behavior.
The guest compiler regression checks argc/argv, environment lookup, and
formatted output through the real libc.

The diagnostic-free compiler gate also exposed intermittent starvation.
Quantum expiry cleared the preferred process, and selection then restarted
at slot zero. Once both the support process and the compiler child reached
priority 48, the earlier support slot won every equal-priority selection.
A controlled QEMU run with `-icount shift=7,sleep=off` reproduced the hang.
GDB snapshots at scheduler ticks 51,824 and 140,645 showed both tasks
runnable at priority 48, the support process selected, and the child's
saved instruction pointer unchanged. A diagnostic change of the support
process priority to 49 released the child and completed the compiler test.
That intervention belongs only to the retained probe. Production selection
uses the last running process as the scan origin when quantum preference
expires; normal priority comparison and unexpired-quantum preference remain.

The disk-layout test separately exceeded its 120-second budget while
booting two libguestfs appliances sequentially. One read-only appliance now
attaches both images and validates each mounted partition independently.
The timeout and image-content assertions remain unchanged. A missing marker
in the second image and a guestfish failure both fail the controller probe.

## Execution contract and replay

The host resolves `/usr/bin/clang` and `/usr/bin/clang++` to Clang 22.1.8,
with `/usr/lib/clang/22/lib/linux/libclang_rt.builtins-i386.a` for target
runtime support. Project implementation builds as C++23 with warnings
treated as errors. The guest uses mksh R59c and dietlibc through the
existing recipes. QEMU is `qemu-system-x86` package `11.1.1-2`:

```text
-machine pc-i440fx-11.1 -accel tcg -cpu 486 -smp 1
-vga std -display none
-netdev user,id=net0,restrict=on -device virtio-net-pci,netdev=net0
```

The disk tests attach VMDK or qcow2 with `snapshot=on`, write COM1 and COM2
transcripts, retain the QEMU command/version and stderr, and compare base
disk SHA-256 before and after each run. The controller sends SIGTERM after
the bounded checks and records any required SIGKILL escalation. Guest
execution, timeout, and controller termination have separate meanings.

```sh
cmake --preset i486-standalone
cmake --build build/i486/Debug \
  --target i486_boot_disk test_i486_dma_pages test_i486_scheduler_wait \
  test_i486_process_sessions test_i486_signal_dispatch \
  test_i486_context_switch test_i486_ata_deadline test_i486_ext2_indirect -j2
ctest --test-dir build/i486/Debug \
  -R '^(test_i486_|i486_.*disk_shell|i486_ata_latency_test)' \
  --output-on-failure
```

CTest creates logs under `build/i486/Debug/logs/i486-*-disk-shell*` and
`build/i486/Debug/logs/i486-disk-shell-*`. The checks require mksh PID 1,
external ELF execution, heap use, a pipeline, ext2 writes, and successful
process-slot reuse with virtio initialized. Command status and output
must both match; echoed commands alone cannot satisfy the output checks.
The departing shell receives a distinct prompt before self-termination, so
only a restarted shell can satisfy the default-prompt readiness check.

The broader QEMU gate includes ISA boot, persistent ext2 mutation, the
compiler/runtime contract, and the instruction-count scheduler stress run:

```sh
ctest --test-dir build/i486/Debug \
  -R '^(test_i486_|i486_|cpp23_c23_assembly_ownership)' \
  -E '^i486_vbox' --output-on-failure
```

`i486_scheduler_fairness_test` runs the two CPU-bound Ring 3 children in
`/bin/preempt_test` with `-icount shift=7,sleep=off`. The test requires each
child to finish and their output to interleave. The enhanced compiler and
ext2 mutation tests separately exercise disk writes. QEMU's PIO write
completion depends on asynchronous host I/O, while this instruction-count
setting advances the guest PIT clock without waiting for host time. Combining
those surfaces made the original fairness test intermittently retire the ATA
device when its three-second PIT deadline expired during a host write.
The focused fairness test retains the accelerated timer and boot deadline
while measuring scheduler progress without a competing host-I/O clock.

## Primary-source hardware boundary

The local QEMU source comparison uses commit
`006a22cb26998998385b104db1ff9466ef2f3153`, described as
`v11.1.0-rc1-33-g006a22cb26`. That source snapshot differs from the installed
11.1.1 package; guest execution results belong to the installed binary.
`hw/i386/pc_piix.c` defines the i440FX machine lineage and `pc` alias;
`target/i386/cpu.c` defines the `486` CPU. In
`hw/virtio/virtio.c::virtqueue_map_desc`, QEMU maps descriptor-provided guest
physical addresses through `dma_memory_map`. A guest process array has
no special protection from that DMA mapping. The driver repair uses
XINIM-owned C++23; QEMU implementation code was used as reference evidence.

## Linked footprint

`llvm-size` reports the following Debug ELF section totals in bytes. The
baseline is the retained pre-repair ELF; the repaired ELF SHA-256 is
`40c3ea505b69ad0febc3709e8b89a51da564de784fd1d70449daf57ee3a9504d`.

| ELF | Text | Data | BSS |
| --- | ---: | ---: | ---: |
| Baseline | 199,011 | 230 | 37,033,184 |
| Repaired | 205,827 | 238 | 37,033,024 |
| Change | +6,816 | +8 | -160 |

The repaired `g_processes` symbol starts at `0x003dbc30` and retains size
`0x02049600`. The linker-owned kernel span ends at `0x02483940`. Session
ownership and continuation repair preserve the measured process-array
size; they provide correctness without claiming a RAM reduction.

## Quality-gate boundary

On 2026-09-22, the diagnostic-free build passed all 30 selected CTest
entries, including two image fixtures, in 790.66 seconds. The command
above covers seven native regressions; VMDK at 64 and 256 MiB; qcow2 and
ISO boot; 16 KiB/s ATA reads; shell, compiler, and scheduler stress;
standard and ISA persistence/mutation; and language/assembly ownership.
The final log is `build/i486/Debug/evidence/release-complete-ctest.log`.
`release-artifact-sha256.txt`, `release-size.txt`, and
`release-ctest-contract.json` retain artifact and harness provenance.
The new shared-ATA gates use CTest `RUN_SERIAL` to prevent concurrent
writers from invalidating their disk observations.

Changed Python passes Ruff, the changed shell controller passes
ShellCheck, changed Markdown passes markdownlint, and the staged diff
passes whitespace checks. Native mutation probes reject the former DMA
overlap, timer poll bound, stale storage callbacks, indirect-block errors,
and scheduler scan origin. Earlier failed and interrupted runs remain in
the evidence directory alongside the final passing run.

The repository was public and Actions enabled at the follow-up check on
2026-09-22. Hosted CI is a separate evidence surface from the local runs.
The broad `.clang-tidy` profile also
rejects both the baseline and repaired allocator, including inherited
`#pragma once`, constant naming, public boot-record fields, and physical
integer/pointer conversions. The probe logs retain those failures.
The repair changes neither that profile nor its warning severity. A
passing compiler, native test, or guest run does not establish a clean
repository-wide clang-tidy or hosted-CI result.

The follow-up Clang 22 `clang-tidy -export-fixes` probe on `ring3.cpp`
recorded 1,099 diagnostics. Exactly 331 diagnostics carry at least one
automatic replacement; 768 require manual analysis. The largest fixable
groups are 141 identifier-name changes, 78 internal-linkage changes, 31
missing-brace changes, and 15 parenthesis changes. Several replacements
target shared C/assembly ABI headers, where changing macros or names can
break consumers even if C++ recompiles. Running `clang-format --dry-run
--Werror` on the translation unit reports many layout differences, but
formatting alone does not repair the semantic diagnostics.

A bounded automation pass can export clang-tidy replacements, select safe
check classes and project-owned C++ paths, apply them in an isolated worktree,
then run `git clang-format` over the resulting diff. The compiler, assembly
ABI tests, native contracts, and exact guest matrix must pass after each
batch. ABI headers and unfixable checks need their own source review. A
whole-file format pass or unfiltered `clang-tidy -fix` would rewrite much
more than the i486 repair and mix C ABI changes into this branch.

A wider local probe on 19 directly changed, compiled C++ sources recorded
4,532 distinct diagnostics after deduplicating shared headers by file,
offset, and check. Exactly 1,001 have replacements and 3,531 require
manual review. The largest groups concern array-to-pointer decay (626),
magic numbers (571), identifier naming (470), constant array indexing
(440), and C arrays (335). The hosted workflow also selects compiled
consumers of changed headers; this source probe is a lower-bound scope
measurement, not a hosted pass or a complete repository-wide count.

Clang 22's `clang-format` supports `InsertBraces: true`. A temporary style
derived from the repository's `.clang-format` and diagnostic-line ranges
from `readability-braces-around-statements` added braces only at the
reported sites. This cleared 39 source-local warnings across `ring3.cpp`
(31), `bootfs.cpp` (4), `kutil.cpp` (3), and `sched.cpp` (1). The i486 kernel
relinked with warnings as errors; the focused process, scheduler,
backing, and assembly ownership contracts and the 64 MiB QEMU disk-shell
test passed. The complete 31-test i486 selection then passed in 789.22
seconds; `build/i486/Debug/evidence/i486-brace-format-final-ctest.log`
retains the result. Two brace warnings in the shared `service_node.hpp` remain
in the `ring3.cpp` consumer; the formatter batch did not change that
header. Whole-file formatting proposed broad unrelated layout and
include-order changes, so the diagnostic ranges are essential to the
bounded method. Remaining semantic diagnostics still require source-level
repairs.

The hosted Clang 22 run at `7e6d3e27` reached every selected compiled source
and reported 5,008 distinct diagnostics after deduplicating repeated header
locations. Five native tests expanded identical `CHECK` macros into 215
`cppcoreguidelines-avoid-do-while` and 215 `modernize-use-std-print`
diagnostics. A shared C++23 test helper now retains the failing expression and
line number, prints through `std::println`, and exits on failure. All five
native tests and a deliberate failing-check fixture pass their contracts;
focused clang-tidy reruns report zero findings from those two checks in the
five test translation units. Clang 22 also supports
`clang-apply-replacements --format --style=file` for a reviewed subset of
exported fixes, which formats only replacement ranges. Identifier renames,
internal-linkage changes, C/assembly ABI arrays, and freestanding bounds
diagnostics require source or policy review before any such batch.
The 19-source probe attributes 458 of its 470 naming findings to modern
`kCamelCase` constexpr names conflicting with the root profile's uppercase
constant rule. Clang 22 probes verify both a `CamelCase` plus `k` prefix rule
and a narrow mixed-convention rule; the repository also contains uppercase
legacy ABI constants, so any naming-policy change needs an ownership boundary
and a full selected-source rerun.

Hosted Clang 22 first found the standalone i486 C++ compile selecting host
libstdc++ headers. The kernel and owned 32-bit user targets now pass
`-stdlib=libc++` only for C++ compilation. Assembly retains its original
command line; the linker retains the freestanding `-nostdlib` boundary.
The final compile database records the header choice for `bootfs.cpp` and
`hello_i486.cpp` and excludes the flag for `entry.S`.

The public runner cannot route to the original mksh R59c HTTP host. The
exact 442,736-byte upstream archive is retained at
`third_party/distfiles/mksh-R59c.tgz` with SHA-256
`77ae1665a337f1c48c61d6b961db3e52119b38e58884d1c89684af31f87bc506`.
The i486 CMake command passes those bytes to the existing digest and archive
member validator. The vendor source is extracted unchanged.

Hosted run `35809495405` then reached Ring 3 but failed 14 of 31 tests.
Its serial logs show repeated invalid-opcode faults at `0x00428429` in
PID 1 and a rescue-shell prompt instead of mksh. The Ubuntu 24.04,
Clang 22.1.8, GNU ld 2.42 reproduction identified an ELF layout cause:
GNU ld assigned an orphan `.note.gnu.build-id` to a writable `PT_LOAD`
starting at `0x00400000`. That load range overlapped the executable
range starting at the same address. `elf32_loader.cpp` copied the
writable range last, replacing executable bytes. The linker script now
places the build-ID note in the executable segment. The reproduced
Ubuntu mksh has disjoint load ranges, and its VMDK boot passes the real
Ring 3 disk-shell checks, including invalid-exec recovery and respawn.

The ELF loader and the pre-image shell verifier reject overlapping
nonempty `PT_LOAD` ranges and entry points outside executable ranges.
The shell verifier requires a loadable `/bin/mksh` and byte-identical
`/bin/mksh`, `/bin/sh`, and `/boot/mksh`. The mksh build runs the verifier
before disk generation, and the disk and ISO rules track the mksh file
so an incremental relink rebuilds the images. A malformed-range copy
fails the verifier; the direct loader contract rejects overlapping and
non-executable-entry fixtures. The Ubuntu incremental build relinked
mksh and regenerated both disk formats after a linker-script touch.

The same hosted run reported `guestfish` failing to start its `supermin`
appliance during the boot-disk layout test. The layout verifier now
checks the VMDK and qcow2 containers with `qemu-img`, validates the MBR
partition and ext2 signature, and reads the required seed files through
read-only `debugfs`. Local and Ubuntu 24.04 runs pass that inspection.

## Bounded architecture result

The initial boot repair preserved static process storage. The follow-up
process-backing change retains one contiguous DMA pool and reserves user
images on admission. The final physical page below 4 GiB remains outside
that pool so
the exclusive end fits its 32-bit state. The Multiboot parser has fixed
capacities of 32 modules and 64 memory ranges; larger descriptions need
explicit truncation handling before broader admission claims. The pinned
QEMU boot reports six memory ranges and the generated disk loads fewer
than 32 modules.

[The process-memory and scheduler comparison](I486_MEMORY_SCHEDULER_DESIGN.md)
records xv6, DiscoBSD, historical BSD, and FUZIX mechanisms, licenses, and
follow-up falsifiers. Runtime image residency and direct Ring 3 isolation
retain separate proof obligations. Shell boot and the bounded tests
establish neither full POSIX conformance nor physical-board behavior.
