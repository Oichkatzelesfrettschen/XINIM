# QEMU x86 PC Source Index

Date: 2026-08-15
Purpose: Official QEMU source cache and driver-plan notes for the XINIM x86
`pc` and `pc-q35-11.1` machine lanes.

## Cached Sources

The HTML files are cached under `data/external/qemu/`; hashes and byte sizes are
recorded in `data/external/qemu/PROVENANCE.json` and
`data/external/qemu/SHA256SUMS`.

The five retained HTML captures range from 14989 to 416377 bytes. They are
hash-manifested source evidence and remain ordinary tracked Git files; Git LFS
is unnecessary at these sizes. Generated QEMU disks, object files, traces, and
logs belong under the ignored `build/` tree unless a separate evidence intake
records their provenance and explicitly admits them.

1. QEMU i440FX PC machine
   - URL: https://www.qemu.org/docs/master/system/i386/pc.html
   - Cache: `data/external/qemu/qemu-master-i386-pc.html`
   - Why it matters: `pc` maps to the i440FX/PIIX legacy PC family. The
     documented device set includes the i440FX host bridge, PIIX3 PCI-to-ISA
     bridge, ISA interrupt/timer plumbing, VGA, serial ports, and two PCI IDE
     interfaces with hard disk and CD-ROM support.

2. QEMU invocation reference
   - URL: https://www.qemu.org/docs/master/system/invocation.html
   - Cache: `data/external/qemu/qemu-master-invocation.html`
   - Why it matters: anchors the exact launch contract: `-machine pc`,
     lane-specific `-cpu`, `-m`, `-serial`, `-drive`, `-boot`, `-d`, `-D`,
     optional `-S`, and optional `-gdb`/`-s`.

3. QEMU disk image reference
   - URL: https://www.qemu.org/docs/master/system/images.html
   - Cache: `data/external/qemu/qemu-master-images.html`
   - Why it matters: confirms that `qcow2` is a primary QEMU disk format and
     that VMDK is a supported compatibility image format. The current XINIM
     lane produces both a dynamic VMDK and qcow2 and gates them with
     `qemu-img check`.

4. QEMU network device reference
   - URL: https://www.qemu.org/docs/master/system/devices/net.html
   - Cache: `data/external/qemu/qemu-master-net.html`
   - Why it matters: establishes the supported `-netdev` plus `-device` model
     split and the available emulated NIC path. For a BSD-licensed guest driver
     build-out, use this as the QEMU-side contract while deriving driver logic
     from compatible code or clean-room register documentation.

5. QEMU GDB usage
   - URL: https://www.qemu.org/docs/master/system/gdb.html
   - Cache: `data/external/qemu/qemu-master-gdb.html`
   - Why it matters: documents the `-s -S` debugging path and remote GDB stub
   behavior used by `scripts/qemu_x86_32_debug.py --gdb`.

## Validated x86_64 Q35 Contract

The exact x86_64 gate runs installed `qemu-system-x86_64` 11.1.0 with:

```text
-machine pc-q35-11.1
-cpu qemu64
-smp 1
-nodefaults
-vga none
-nic none
```

The local primary-source checkout used for this contract is QEMU commit
`006a22cb26998998385b104db1ff9466ef2f3153`, described as
`v11.1.0-rc1-33-g006a22cb26`.

### Machine and interrupt topology

- `hw/i386/pc_q35.c` registers `pc-q35-11.1` through
  `DEFINE_Q35_MACHINE_AS_LATEST(11, 1)`. Its machine description is
  `Standard PC (Q35 + ICH9, 2009)`.
- `pc_q35_init()` creates the Q35 PCIe host bus, an ICH9 LPC function, the
  LPC-owned `isa.0` child bus, 24 GSI lines, and an IOAPIC. The ICH9 LPC
  routes ISA interrupts 0 through 15 and PCI PIRQ A through H to IOAPIC GSIs
  16 through 23 in APIC mode.
- `include/hw/intc/ioapic.h` fixes the primary IOAPIC MMIO base at
  `0xfec00000` and its input count at 24. `hw/intc/ioapic.c` models the
  redirection table and edge-versus-level delivery behavior the guest must
  program and acknowledge correctly.
- `hw/i386/acpi-common.c` emits the MADT with the local APIC base, the primary
  IOAPIC at `0xfec00000`, the legacy IRQ0-to-GSI2 interrupt-source override,
  and level-triggered PCI interrupt overrides. XINIM should consume MADT
  topology as the authority and keep hard-coded addresses only as validated
  early-boot defaults.

### Timers and serial devices

- `hw/i386/pc.c` enables HPET by default for PC machines. Q35 passes interrupt
  capability mask `0xff0104`, which permits GSIs 16 through 23 plus IRQ8 and
  IRQ2, and maps the HPET at `0xfed00000`.
- `include/hw/timer/hpet.h` defines a 10 ns counter period. `hw/timer/hpet.c`
  publishes that period in the capability register. XINIM's LAPIC calibration
  therefore uses the emulated counter as a measured timebase, not a guessed
  CPU frequency.
- `hw/char/serial-isa.c` assigns COM1 through COM4 to I/O bases `0x3f8`,
  `0x2f8`, `0x3e8`, and `0x2e8`, with IRQs 4, 3, 4, and 3. The XINIM gate
  explicitly creates COM1 for kernel output and COM2 for the Ring 3 terminal,
  so the driver must route IRQ4 and IRQ3 through the Q35 GSI/IOAPIC path.

### CPU, storage, and explicit device absence

- `target/i386/cpu.c` defines `qemu64` as an AMD-vendor family 15 model with
  long mode, `SYSCALL`, NX, MTRR, CLFLUSH, MCA, PSE36, SSE3, CX16, LAHF in
  long mode, and SVM. Drivers and userspace must stay within that declared
  feature boundary; host CPU features are not part of the gate.
- `pc_q35_init()` creates the six-port ICH9 AHCI controller when SATA is
  enabled. `hw/ide/ich.c` registers the index/data port in PCI BAR4 and the
  AHCI MMIO register block in PCI BAR5. A clean-room guest storage driver must
  enumerate PCI, validate the ICH9 AHCI function, read BAR5, establish DMA
  ownership, and test command/FIS/interrupt completion instead of assuming a
  fixed BAR address.
- QEMU's `qemu-options.hx` states that `-nodefaults` disables default serial,
  parallel, console, monitor, VGA, floppy, CD-ROM, and other devices. It does
  not remove machine-owned Q35 chipset functions created by `pc_q35_init()`.
  The gate adds its two serial devices and CD-ROM explicitly.
- Although Q35's class default NIC is E1000e, the gate also passes
  `-nic none`. No E1000e or virtio-net device exists in this platform contract.
  Network driver work must add one explicit `-device` plus `-netdev` pair and
  a matching PCI and packet-I/O test before claiming that transport.

These QEMU files are behavioral evidence, not guest implementation sources.
Do not copy QEMU implementation code into XINIM. Build C++23 guest drivers from
the device specifications and validate them against the versioned model.

## Driver And Subsystem Implications

- CPU lanes: the existing `i486`, `i586`, and `i686` lanes map to QEMU `486`,
  `pentium`, and `pentium3` respectively. The CMOV-capable lane is the `i686`
  lane, because QEMU's Pentium-class lane is pre-CMOV.
- Machine model: the 32-bit lanes should keep `-machine pc` as the stable
  bring-up target. That gives the kernel legacy PIC/PIT/ISA serial/VGA/IDE
  devices before higher-variance PCI device work.
- Block driver: the primary boot device should remain IDE/ATA on the `pc`
  machine. The driver contract is ATA register probing plus partition/ext2
  validation, then image-format coverage through QEMU dynamic VMDK and qcow2.
- Serial/TTY: COM1 is the kernel log channel and COM2 is the interactive shell
  test channel. Driver debugging should keep serial markers independent of VGA.
- PCI: enumerate PCI on the `pc` machine before binding optional devices. Treat
  PCI discovery as a prerequisite for virtio or e1000-class NIC work.
- Network: prefer a staged path. First add PCI enumeration assertions, then a
  simple emulated NIC probe, then packet I/O. Use virtio-net only after the
  guest has virtqueue/DMA support; otherwise use it as a presence/feature probe.
- Debugging: use `-d int,cpu_reset,guest_errors,unimp -D <trace>` for exception
  and device faults, and use `-s -S` when register-level stop-and-inspect is
  needed.

## Clean-Room Reverse-Engineering Rules

- Official QEMU docs and `qemu-system-i386 -device help` are the hardware
  contract for this repo.
- Prefer BSD, ISC, MIT, public-domain, or repo-owned sources for driver
  structure and tests.
- GPL or license-unclear code may be used only for behavioral comparison unless
  a separate license review permits adaptation.
- Local journal paths, personal project notes, and raw local repo scans stay
  under ignored build logs, not tracked source docs.

## Evidence Anchors

- [X86CpuLanes.cmake](../../cmake/X86CpuLanes.cmake)
- [qemu_i486.sh](../../scripts/qemu_i486.sh)
- [qemu_matrix.py](../../scripts/qemu_matrix.py)
- [qemu_x86_32_debug.py](../../scripts/qemu_x86_32_debug.py)
- [QEMU_X86_32_DEBUGGING.md](../testing/QEMU_X86_32_DEBUGGING.md)
