.section .text
.global _start
.type _start, @function

.set XINIM_SYS_exit, 60

_start:
    xor %rdi, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    call xinim_user_main
    mov %rax, %rdi
    mov $XINIM_SYS_exit, %rax
    syscall

1:
    hlt
    jmp 1b
