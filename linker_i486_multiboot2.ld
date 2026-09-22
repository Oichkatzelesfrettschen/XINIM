OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 1M;
    __xinim_kernel_start = .;

    .multiboot2_header ALIGN(8) : {
        KEEP(*(.multiboot2_header))
    } :text

    .text ALIGN(16) : {
        *(.text .text.*)
    } :text

    .rodata ALIGN(16) : {
        *(.rodata .rodata.*)
    } :text

    .data ALIGN(16) : {
        *(.data .data.*)
    } :data

    .bss ALIGN(16) : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    __xinim_kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.eh_frame_hdr*)
    }
}
