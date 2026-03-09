/* Xinim Kernel Linker Script - Limine higher-half x86_64 kernel layout */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS {
    requests PT_LOAD FLAGS(4);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

_kernel_physical_start = 1M;
_kernel_virtual_base = 0xffffffff80000000;

SECTIONS {
    . = _kernel_virtual_base + _kernel_physical_start;
    _kernel_virtual_start = .;

    /* Limine must see a higher-half ELF image, not a low 1 MiB VMA binary. */
    .limine_requests ALIGN(4K) : AT(ADDR(.limine_requests) - _kernel_virtual_base) {
        KEEP(*(.limine.requests))
    } :requests

    .multiboot_header ALIGN(4K) : AT(ADDR(.multiboot_header) - _kernel_virtual_base) {
        *(.multiboot_header)
    } :requests

    .text ALIGN(4K) : AT(ADDR(.text) - _kernel_virtual_base) {
        *(.text .text.* .gnu.linkonce.t.*)
    } :text

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - _kernel_virtual_base) {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } :rodata

    .data ALIGN(4K) : AT(ADDR(.data) - _kernel_virtual_base) {
        *(.data .data.* .gnu.linkonce.d.*)
    } :data

    .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - _kernel_virtual_base) {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
    } :data

    _kernel_virtual_end = ALIGN(4K, .);
    _kernel_physical_end = LOADADDR(.bss) + SIZEOF(.bss);
}
