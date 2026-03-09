.section .text
.global _start
.type _start, @function

.set XINIM_SYS_exit, 25

_start:
    mov (%esp), %eax
    lea 4(%esp), %edx
    lea 4(%edx, %eax, 4), %ecx
    push %ecx
    push %edx
    push %eax
    call xinim_user_main
    add $12, %esp
    mov %eax, %ebx
    mov $XINIM_SYS_exit, %eax
    int $0x80

1:
    hlt
    jmp 1b
