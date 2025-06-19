section .multiboot_header
header_start:
    dd 0xe85250d6                ; Multiboot2 magic number (MBOOT2_HEADER_MAGIC)
    dd 0                         ; Architecture (protected mode i386)
    dd header_end - header_start ; Header length
    dd -(0xe85250d6 + 0 + (header_end - header_start)) ; Checksum

    ; Required tags
    ; Information request tag
    dw 1                         ; Type (MBI_TAG_TYPE_INFO_REQUEST)
    dw 0                         ; Flags
    dd 8                         ; Size
    ; dd 0 ; Optional: mbi_tag_types[]

header_end:

section .text
global _start
extern kmain

_start:
    cli ; Disable interrupts

    ; Set up a stack
    mov esp, stack_top

    ; Push Multiboot2 info to stack (already there from GRUB)
    ; call kmain
    push ebx ; Multiboot2 info structure address
    push eax ; Multiboot2 magic number
    call kmain

    ; Halt loop
.halt:
    hlt
    jmp .halt

section .bss
resb 8192 ; Reserve 8KB for the stack
stack_bottom:
stack_top:
