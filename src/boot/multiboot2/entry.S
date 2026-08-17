.section .multiboot2_header, "a"
.align 8
.global multiboot2_header_start
.type multiboot2_header_start, @object
multiboot2_header_start:
    .long 0xe85250d6
    .long 0
    .long multiboot2_header_end - multiboot2_header_start
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start))

    .short 0
    .short 0
    .long 8
multiboot2_header_end:

.section .bss, "aw", @nobits
.align 16
boot_stack_bottom:
    .skip 65536
boot_stack_top:

.section .text, "ax"
.global _start
.type _start, @function
.extern xinim_i486_kmain
_start:
    cli
    cld
    movl $boot_stack_top, %esp
    subl $8, %esp
    pushl %ebx
    pushl %eax
    call xinim_i486_kmain

1:
    cli
    hlt
    jmp 1b
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
