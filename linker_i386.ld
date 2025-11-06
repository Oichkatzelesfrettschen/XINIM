/*
 * i386 (32-bit x86) Linker Script for XINIM
 * 
 * This linker script configures the kernel for 32-bit i386 architecture.
 * Physical load address: 1MB (standard for multiboot kernels)
 * Virtual address: same as physical (identity mapping initially)
 */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

_kernel_physical_start = 1M;
_kernel_virtual_start = 1M;

SECTIONS {
    . = 1M; /* Load at 1MB physical address */

    /* Multiboot header must be in first 8KB */
    .multiboot_header ALIGN(4K) : {
        *(.multiboot_header)
    }

    /* Kernel code section - executable */
    .text ALIGN(4K) : {
        *(.text .text.* .gnu.linkonce.t.*)
        *(.init)
        *(.fini)
    }

    /* Read-only data section */
    .rodata ALIGN(4K) : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }

    /* Exception handling and unwinding info */
    .gcc_except_table ALIGN(4K) : {
        *(.gcc_except_table .gcc_except_table.*)
    }

    /* Constructor/destructor arrays for C++ */
    .ctors ALIGN(4) : {
        __CTOR_LIST__ = .;
        KEEP(*(.ctors))
        KEEP(*(.init_array))
        LONG(0)
        __CTOR_END__ = .;
    }

    .dtors ALIGN(4) : {
        __DTOR_LIST__ = .;
        KEEP(*(.dtors))
        KEEP(*(.fini_array))
        LONG(0)
        __DTOR_END__ = .;
    }

    /* Initialized read-write data */
    .data ALIGN(4K) : {
        *(.data .data.* .gnu.linkonce.d.*)
        *(.got .got.plt)
    }

    /* Uninitialized data (zeroed at load) */
    .bss ALIGN(4K) : {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
    }

    /* End markers for kernel memory */
    _kernel_physical_end = ALIGN(4K, .);
    _kernel_virtual_end = _kernel_virtual_start + (_kernel_physical_end - _kernel_physical_start);

    /* Discard debugging symbols in release builds */
    /DISCARD/ : {
        *(.comment)
        *(.note.GNU-stack)
    }
}
