ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00400000;

    .text : ALIGN(16) {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    } :text

    .note.gnu.build-id : ALIGN(4) {
        *(.note.gnu.build-id)
    } :text

    .data : ALIGN(16) {
        *(.data .data.*)
    } :data

    .bss (NOLOAD) : ALIGN(16) {
        *(.bss .bss.*)
        *(COMMON)
    } :data
}
