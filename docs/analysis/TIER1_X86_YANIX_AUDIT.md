# Tier I x86 and YANIX Audit

Date: 2026-03-11

This note audits where XINIM currently stands against the goal of Tier I
`i386` and `x86_64` support, QEMU-first HAL bring-up, practical
`E1000`/`AHCI`/`VirtIO` driver work, stronger VFS and server plumbing, and
selective design borrowing from `RobbeDGreef/yanix`.

## Executive Read

The short version is:

- XINIM is already strongest on the low-end `i386` path, especially the i486
  QEMU lane.
- XINIM is weaker on the shared x86 HAL story than the docs sometimes imply.
- `x86_64` bring-up works well enough for staged shell validation, but not yet
  as a boring, generic, Tier I UNIX-like baseline.
- The repo currently mixes three filesystem and service models:
  - freestanding i486 bootfs plus ext2 promotion
  - bare-metal in-kernel ramfs/VFS service loops
  - a separate userspace-style VFS server tree
- `yanix` is useful mainly as a donor for subsystem boundaries and packaging
  shape, not as a direct implementation donor.

## Where XINIM Actually Is

### Strongest current lane: i486 / low-end x86

The i486 boot lane is the most honest foundation for Tier I x86 support.

- `docs/CURRENT_REALITY.md` says the verified lane matrix includes `x86_64`,
  `i486`, `i586`, `i686`, and multiple 32-bit tuning lanes.
- The same document says the i486 lane is verified on `pc-i440fx-10.2` and
  `isapc` with `-cpu 486`.
- `src/kernel/i486/main.cpp` shows a concrete freestanding boot path:
  boot info handoff, bootfs promotion, IDE init, ext2 probe, and supervised
  Ring 3 shell launch.
- `src/kernel/i486/ide.cpp` is already the narrow kind of port-based ATA PIO
  driver that matches a real low-end QEMU baseline.
- `test/boot/x86_32_smoke_test.sh` already codifies the right default machine
  contract for the 32-bit lane: `-machine pc`, `-cpu 486`, with ATA and ext2
  evidence checks when a disk image is attached.

This is the lane to treat as Tier I first, because it already matches the
smallest believable x86 PC model.

### x86_64 works, but is not yet a clean Tier I platform lane

The `x86_64` lane boots and reaches a staged shell, but its hardware and
service story is still partial.

- `src/kernel/main.cpp` boots through Limine, probes ACPI, calibrates LAPIC
  plus HPET, initializes GDT/TSS/IDT, then launches the staged x86_64 shell.
- The same file only performs PCI enumeration plus `virtio_net_init()` after
  the shell handoff. It does not bring up `E1000` or `AHCI`.
- `scripts/qemu_x86_64.sh` defaults to `-machine q35 -cpu qemu64`, but also
  injects an `ahci` device and an `e1000` NIC into the VM.
- That means the default QEMU x86_64 launcher and the kernel bring-up sequence
  are mismatched today: the VM offers `AHCI` and `E1000`, while the kernel only
  tries a partial `virtio-net` path.
- `test/boot/x86_64_shell_test.py` validates the shell contract, not a stable
  storage or networking contract.

This is good bring-up progress, but it is still a shell-first debug lane, not
yet a stable generic-platform Tier I lane.

### VFS and server plumbing are still split across incompatible models

The current storage and service story is the hardest architectural issue in the
repo.

- `src/kernel/bare_metal_stubs.cpp` runs VFS in a bare-metal in-kernel service
  loop and leaves PM and MM as reply-only stubs.
- `src/servers/vfs_server/main.cpp` is a separate userspace-style VFS server
  built around `std::unordered_map`, `std::shared_ptr`, and ramfs logic.
- The i486 lane separately mounts and probes ext2 through the freestanding
  `src/kernel/i486/ext2_reader.cpp` path.

So right now XINIM does not have one unified answer to:

1. where the authoritative VFS core lives,
2. whether filesystem service logic is in-kernel or message-routed,
3. how the i486 ext2 path graduates into the same model used by x86_64.

Until that is unified, broad UNIX-like growth will keep getting more expensive.

## Driver Audit Against The Stated Goal

### IDE/ATA on i386

Status: the best current fit.

- The repo docs already chose IDE/ATA PIO as the correct first storage target
  for the i486 QEMU lane.
- The current i486 code matches that choice.
- This path aligns with `pc` / i440FX / PIIX style emulation and the oldest,
  slowest machine concept you asked for.

Conclusion:

- Keep ATA PIO as the Tier I `i386` storage baseline.
- Do not make AHCI, `virtio-blk`, or networking a prerequisite for the i386
  baseline.

### VirtIO

Status: partial enumeration, not a working network path.

- `src/drivers/net/virtio_net.cpp` can find the PCI function, read BAR0,
  negotiate a tiny feature set, read the MAC, and record queue sizes.
- It explicitly stops before queue allocation and does not set `DRIVER_OK`.
- `virtio_net_send()` and `virtio_net_recv()` both still return failure.

Conclusion:

- Keep VirtIO as a separate follow-on driver lane after the generic platform is
  stable.
- It is a good QEMU optimization path, but not the right first definition of
  Tier I x86 success.

### E1000

Status: driver body exists, platform integration does not.

- `src/drivers/net/e1000.cpp` contains a substantial driver implementation.
- It still depends on the MMIO base being configured elsewhere.
- There is no current x86_64 boot path that wires PCI discovery, BAR mapping,
  IRQ routing, and net stack plumbing into that driver.

Conclusion:

- `E1000` is currently better described as a partially implemented driver body
  than as a supported device lane.
- It should become the first serious x86_64 NIC target only after the generic
  PCI plus IRQ plus DMA path is made architecture-neutral.

### AHCI

Status: good skeleton, not a finished bring-up path.

- `src/drivers/block/ahci.cpp` has controller probing and per-port setup
  structure.
- It still depends on externally configured ABAR setup.
- It still skips real IRQ registration in the current path.

Conclusion:

- AHCI should not define the first x86 baseline.
- It is appropriate as a modern x86_64 storage lane after the generic `pc`
  baseline is stable.

## The Biggest Cross-Arch Problem

The worst technical issue is not any single driver. It is that the current
cross-arch HAL line is still leaky.

Examples:

- `src/pci/pci.cpp` pulls in `src/kernel/arch/x86_64/portio.hpp` directly.
- `src/drivers/net/virtio_net.cpp` also binds itself to the x86_64 port-I/O
  header.
- The i486 lane contains its own local ATA and ext2 path instead of consuming a
  clearly shared x86 bus plus storage layer.

That means XINIM is not yet using one real x86 HAL with thin `i386` and
`x86_64` adapters. It is still using lane-specific bring-up islands.

## Tier I Recommendation

### Tier I i386 baseline

Use the oldest, slowest credible machine:

- Emulator: `qemu-system-i386`
- Machine: `pc` first
- CPU: `486`
- Memory: `32M`
- Storage baseline: ATA PIO on the primary IDE path
- Console baseline: serial plus VGA text plus PS/2 keyboard

Compatibility lane after the main one is green:

- `isapc`

Why:

- This matches the repo's strongest current lane.
- It keeps the storage story honest.
- It avoids hiding missing generic platform work behind newer virtual devices.

### Tier I x86_64 baseline

Use the generic PC first, not `q35`.

- Emulator: `qemu-system-x86_64`
- Machine: `pc`
- CPU: `qemu64`
- Memory: `256M` to `512M`
- SMP: `1`
- Storage baseline: start with boot modules plus simple block path, then add one
  modern storage device after the shared PCI and interrupt path is clean

Why:

- `pc` is the generic i440FX path in local QEMU 10.2.0 on both `i386` and
  `x86_64`.
- It reduces chipset variance between the Tier I 32-bit and 64-bit lanes.
- It makes a shared x86 HAL easier to prove before introducing `q35` and ICH9.

### Non-Tier-I follow-on lanes

After the generic baseline is boring:

- modern x86_64 lane: `q35 + qemu64`
- NIC lane: `E1000`
- storage lane: `AHCI`
- paravirtual lane: `VirtIO`

Those are important, but they should be treated as feature lanes layered on top
of Tier I, not as the definition of Tier I.

## What To Borrow From YANIX

`yanix` is useful as a donor in five narrow ways.

### 1. Packaging shape

Its top-level split is good:

- `kernel/`
- `libs/`
- `sysroot/`
- `system/`
- `apps/`
- `tools/`

XINIM should mirror this idea more explicitly in its build artifacts and staged
images, even while keeping the current CMake and Conan flow.

### 2. Dedicated driver-to-disk abstraction

`yanix` has a clear disk abstraction layer between ATA and filesystem code:

- `kernel/drivers/ata/`
- `kernel/drivers/disk.c`
- `kernel/include/drivers/disk.h`

That is worth copying at the design level into pure C++:

- one block device contract
- one partition view contract
- one filesystem-facing byte and block adapter

### 3. VFS shape, not code

`yanix` keeps `fs/ext2`, `fs/vfs`, and descriptor handling close together. The
implementation style is older and not something XINIM should transliterate, but
the shape is instructive:

- one obvious VFS layer
- one obvious descriptor layer
- one obvious filesystem registration path

XINIM needs that clarity badly.

### 4. Userland staging model

`yanix` separates shell tools and system binaries under `system/`. XINIM should
keep its C++23 userland, but adopt the same discipline around:

- shell baseline tools
- sysroot population
- packaged system image content

### 5. Syscall breadth checklist

`yanix` has an immediately visible syscall surface under
`kernel/proc/syscalls/`. That is useful as an audit checklist for XINIM's
UNIX-like completeness, even if the implementation style should not be copied.

## What Not To Borrow From YANIX

This is the "worst of it" from a porting perspective.

Do not directly port these into C++:

- the bootloader
- the scheduler and tasking internals
- the older ext2 implementation
- the ad hoc shell
- the syscall implementation bodies
- broad kernel-global mutable state patterns

Reasons:

- XINIM already has different boot assumptions on both lanes.
- The upstream README explicitly does not claim `x86-64` support.
- The upstream TODO calls out threading and ext2 messiness.
- A direct C-to-C++ translation would import old design debt faster than it
  would import useful capability.

The safe rule is:

- borrow subsystem boundaries,
- borrow packaging ideas,
- borrow checklists,
- do not borrow implementation style unless a file is clearly exceptional.

## Concrete Next Sequence

1. Declare Tier I `i386` as `qemu-system-i386 -machine pc -cpu 486`.
2. Declare Tier I `x86_64` as `qemu-system-x86_64 -machine pc -cpu qemu64`.
3. Refactor x86 port I/O and PCI enumeration into a shared x86 HAL layer that
   both `i386` and `x86_64` consume.
4. Lift the i486 ATA path behind the repo-wide block-device abstraction.
5. Choose one authoritative VFS core and make both freestanding lanes route
   through it.
6. Make PM and MM something more real than stub reply loops.
7. Add a generic x86_64 boot smoke test on `pc`, separate from the current
   `q35` shell lane.
8. After that, add feature lanes in this order:
   - `E1000`
   - `AHCI`
   - `VirtIO`

## Bottom Line

If the goal is a compact, modern, proper UNIX-like OS, the shortest honest path
is not "implement every interesting QEMU device now." It is:

- stabilize the low-end i386 lane that already works,
- force x86_64 onto the same generic PC baseline,
- unify the x86 HAL and VFS story,
- then grow modern drivers as feature lanes.

That gives XINIM a real Tier I core instead of two partially overlapping bring-up
stories.
