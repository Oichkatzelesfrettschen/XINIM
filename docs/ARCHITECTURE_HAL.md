# XINIM Architecture: x86_64 Focused HAL

This document outlines the architecture for XINIM kernel in C++23, with a well-defined Hardware Abstraction Layer (HAL) focused exclusively on x86_64 and a standards-compliant userspace ABI.

## Goals
- x86_64 focus: optimized for modern 64-bit Intel/AMD processors
- Boot flexibility: support for Limine/Multiboot2 and UEFI on x86_64
- Pure C++23 core with C ABI surface for POSIX C17; no RTTI/exceptions across ABI
- Determinism: explicit initialization order, no static init surprises
- SIMD optimization: AVX2/AVX512 vectorization where beneficial

## Layering
- Core kernel: scheduler, VFS, IPC, VM policy, drivers' logical models
- HAL (x86_64): CPU, MMU/paging, interrupts, timers, PCI/ACPI, APIC/IOAPIC, console, block/NIC bring-up
- Boot interface: normalized handoff (mem map, firmware tables, modules) from Limine/UEFI/Multiboot2

## HAL Interfaces (x86_64-specific)
- CPU: cpuid, barriers, tsc/rdtsc, pause, SMP bring-up hooks, SIMD detection
- MMU: 4-level page tables (PML4), TLB shootdowns, VA/PA conversions, huge pages
- IRQ: vector allocation, mask/unmask, EOI, ISRs registration, MSI/MSI-X
- Timers/Clock: TSC, HPET, APIC timer, monotonic wallclock
- Bus: PCIe enumeration, config space, MSI/MSI-X setup
- Platform: ACPI tables, APIC/IOAPIC init, HPET configuration
- IO: serial/VGA console; storage (AHCI); network (e1000/virtio-net) as reference devices

## Boot Strategy
- Preferred: Limine (robust on BIOS/UEFI, x86_64). Alternate: Multiboot2, direct UEFI PE/COFF
- Handoff: normalize into `BootInfo` (mem map, modules, rsdp, cmdline) consumed by core
- 64-bit long mode entry point

## POSIX & libc Strategy
- Provide C ABI (`extern "C"`) implemented in C++23, compiled with `-fno-exceptions -fno-rtti` for kernel
- Conformance plan (SUSv5/POSIX 2024): phase 1 (process, files, time), phase 2 (signals, sockets), phase 3 (threads, aio)
- Map syscalls to C wrappers in a `libc` tree

## Next Steps
1) Validate HAL interfaces (headers) and x86_64 implementation
2) Define `BootInfo` and a Limine/UEFI probe shim
3) Integrate clock/irq primitives and wire early console + timer tick in QEMU
4) Build user ABI shim and syscalls to validate end-to-end
