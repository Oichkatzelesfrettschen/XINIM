# XINIM Architecture: ISA‑Independent Core with HAL

This document outlines a pragmatic path to a fully ISA‑independent kernel in C++23, with a small, well‑defined Hardware Abstraction Layer (HAL) and a standards‑compliant userspace ABI.

## Goals
- ISA independence: clean separation between core kernel logic and platform specifics.
- Boot flexibility: support for Limine/Multiboot2 and UEFI on i386/x86_64.
- Pure C++23 core with C ABI surface for POSIX C17; no RTTI/exceptions across ABI.
- Determinism: explicit initialization order, no static init surprises.

## Layering
- Core kernel: scheduler, VFS, IPC, VM policy, drivers’ logical models.
- HAL (per‑arch, per‑platform): CPU, MMU/paging, interrupts, timers, PCI/ACPI, APIC/IOAPIC, console, block/NIC bring‑up.
- Boot interface: normalized handoff (mem map, firmware tables, modules) from Limine/UEFI/Multiboot2.

## HAL Interfaces (high‑level)
- CPU: cpuid, barriers, tsc/rdtsc, pause, SMP bring‑up hooks.
- MMU: page tables (create/map/unmap), TLB shootdowns, VA/PA conversions.
- IRQ: vector allocation, mask/unmask, EOI, ISRs registration.
- Timers/Clock: monotonic wallclock, high‑res timer, scheduler tick.
- Bus: PCIe enumeration, config space, MSI/MSI‑X setup.
- Platform: ACPI tables, APIC/IOAPIC init, HPET.
- IO: serial/VGA console; storage (AHCI); network (e1000/virtio‑net) as reference devices.

## Boot Strategy
- Preferred: Limine (robust on BIOS/UEFI, i386/x86_64). Alternate: Multiboot2, direct UEFI PE/COFF.
- Handoff: normalize into `BootInfo` (mem map, modules, rsdp, cmdline) consumed by core.

## POSIX & libc Strategy
- Provide C ABI (`extern "C"`) implemented in C++23, compiled with `-fno-exceptions -fno-rtti` for kernel.
- Conformance plan (SUSv5/POSIX 2024): phase 1 (process, files, time), phase 2 (signals, sockets), phase 3 (threads, aio). Map syscalls to C wrappers in a `libc` tree.

## Next Steps
1) Land HAL interfaces (headers) and x86_64 skeleton library.
2) Define `BootInfo` and a Limine/UEFI probe shim (out‑of‑tree until vendored).
3) Integrate clock/irq primitives and wire a minimal early console + timer tick in QEMU.
4) Build a tiny user ABI shim and a couple of syscalls to validate end‑to‑end.

