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
        *(.note.gnu.build-id)
    } :text

    /* Separate RX and RW pages so the loader can enforce W xor X. */
    . = ALIGN(0x1000);
    .data : ALIGN(16) {
        *(.data .data.*)
    } :data

    .bss (NOLOAD) : ALIGN(16) {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    ASSERT((ADDR(.data) & 0xfff) == 0,
           "x86_64 user data segment is not page aligned")
}
