# Xinim Boot Sequence

Status: Skeleton (Phase 6 will fill in)

## Overview

1. Limine bootloader loads kernel ELF at 1MB physical.
2. `_start` (src/boot/limine/shim.cpp) receives Limine boot context.
3. Kernel parses Limine requests: memory map, framebuffer, SMP.
4. Early serial console initialized (COM1 @ 0x3F8, 115200 baud).
5. GDT loaded (src/kernel/arch/x86_64/gdt.cpp).
6. IDT loaded with exception handlers.
7. Paging initialized (identity map + higher-half).
8. BSS cleared.
9. Kernel main() entered (src/kernel/main.cpp).
10. APIC, HPET, IOAPIC initialized.
11. Scheduler started.
12. Server processes spawned (VFS, PM, MM -- Ring 0 stubs).
13. kshell started on COM2.

## Gaps (to be addressed in Phase 6-7)

- Ring 3 transition not implemented.
- Server message loops not activated.
- Ramfs not mounted.

## Key Files

- `src/boot/limine/shim.cpp` - Limine entry, jumps to kernel main
- `src/boot/limine/limine_boot.cpp` - Boot info parsing
- `src/kernel/main.cpp` - Kernel initialization sequence
- `src/kernel/early/serial_16550.cpp` - Early serial console
- `src/kernel/arch/x86_64/gdt.cpp` - GDT setup
- `linker.ld` - Kernel memory layout
