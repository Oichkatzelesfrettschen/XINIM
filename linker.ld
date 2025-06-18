ENTRY(_start)

_kernel_physical_start = 1M;
_kernel_virtual_start = 1M; /* Kernel expects to run from this virtual address */

SECTIONS {
    . = 1M; /* Set current location counter to 1MB (physical load address) */

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
    /* ALIGN(4K) ensures _kernel_physical_end starts on a page boundary if BSS is empty, */
    /* or that BSS itself starts on a page boundary. */
    .bss ALIGN(4K) : {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
    }

    /* Symbol marking the end of the kernel's physical memory usage */
    /* For identity mapping, this is also the end of its virtual space from its perspective. */
    _kernel_physical_end = ALIGN(4K, .); /* Align end to next page boundary for safety */
    _kernel_virtual_end = _kernel_virtual_start + (_kernel_physical_end - _kernel_physical_start);

}
