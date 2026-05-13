# QEMU x86 PC Source Index

Date: 2026-05-13
Purpose: Official QEMU source cache and driver-plan notes for the 32-bit
XINIM `pc` machine lanes, especially the i686 CMOV-capable disk boot path.

## Cached Sources

The HTML files are cached under `data/external/qemu/`; hashes and byte sizes are
recorded in `data/external/qemu/PROVENANCE.json` and
`data/external/qemu/SHA256SUMS`.

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

- [X86CpuLanes.cmake](/home/eirikr/Github/XINIM/cmake/X86CpuLanes.cmake)
- [qemu_i486.sh](/home/eirikr/Github/XINIM/scripts/qemu_i486.sh)
- [qemu_matrix.py](/home/eirikr/Github/XINIM/scripts/qemu_matrix.py)
- [qemu_x86_32_debug.py](/home/eirikr/Github/XINIM/scripts/qemu_x86_32_debug.py)
- [QEMU_X86_32_DEBUGGING.md](/home/eirikr/Github/XINIM/docs/testing/QEMU_X86_32_DEBUGGING.md)
