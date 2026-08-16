#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    mov $6, %rax
    lea ring3_entry_message(%rip), %rdi
    mov %rdi, %rsi
    mov $1, %rdi
    mov $(ring3_entry_message_end - ring3_entry_message), %rdx
    syscall

    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    lea 16(%rsp,%rdi,8), %rdx
    call xinim_user_main
    mov %rax, %rdi
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall

1:
    hlt
    jmp 1b

.size _start, . - _start

.section .rodata
ring3_entry_message:
    .ascii "[user] Ring 3 xash entry\n"
ring3_entry_message_end:

.section .note.GNU-stack,"",@progbits
