/* Xinim Kernel Linker Script - Limine boot protocol (x86-64, 1MB physical load) */
/* WHY: Merged Limine protocol sections from src/kernel/linker.ld so that both
        the Limine .requests section and legacy multiboot header are covered.
        src/kernel/linker.ld is now superseded by this file. */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

_kernel_physical_start = 1M;
_kernel_virtual_start = 1M;

SECTIONS {
    . = 1M;

    /* Limine boot protocol request section (must appear before .text) */
    .limine_requests ALIGN(4K) : {
        KEEP(*(.limine.requests))
    }

    /* Legacy multiboot header (kept for compatibility) */
    .multiboot_header ALIGN(4K) : {
        *(.multiboot_header)
    }

    /* Kernel code */
    .text ALIGN(4K) : {
        *(.text .text.* .gnu.linkonce.t.*)
    }

    /* Kernel read-only data */
    .rodata ALIGN(4K) : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Kernel read-write data */
    .data ALIGN(4K) : {
        *(.data .data.* .gnu.linkonce.d.*)
    }

    /* Kernel uninitialized data */
    .bss ALIGN(4K) : {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
    }

    /* Symbols marking the end of kernel physical/virtual memory usage */
    _kernel_physical_end = ALIGN(4K, .);
    _kernel_virtual_end = _kernel_virtual_start + (_kernel_physical_end - _kernel_physical_start);
}
