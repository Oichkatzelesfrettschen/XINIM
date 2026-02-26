# Xinim Driver Interface

Status: Skeleton (Phase 6-7 will expand)

## Overview

Drivers in Xinim run in Ring 0 (kernel space) during the current phase.
The long-term goal is to move drivers to Ring 3 user processes communicating
via IPC (microkernel design).

## Current Driver Inventory

| Driver | File | Status |
|--------|------|--------|
| Serial 16550 | src/kernel/early/serial_16550.cpp | Functional (COM1 log, COM2 kshell) |
| APIC | src/hal/x86_64/hal/apic.cpp | Functional |
| IOAPIC | src/hal/x86_64/hal/ioapic.cpp | Stub |
| HPET | src/hal/x86_64/hal/hpet.cpp | Stub |
| PCI | src/hal/x86_64/hal/pci.cpp | Stub |
| Floppy | src/kernel/floppy.cpp | Stub |
| Wini (disk) | src/kernel/wini.cpp | Stub |
| VirtIO-net | (planned Phase 7) | Not started |

## HAL Interface

The Hardware Abstraction Layer (src/hal/) provides:
- `hal::memory::*` - Physical/virtual memory management
- `hal::arch::*` - CPU-specific operations (CPUID, MSR, RDRAND)
- `hal::io::*` - Port I/O primitives

## Future Work

- Ring 3 driver isolation via IPC
- VirtIO-net skeleton (Phase 7)
- Disk driver (ramfs/ext2) for VFS backing
