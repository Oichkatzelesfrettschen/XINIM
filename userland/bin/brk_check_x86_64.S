#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    xor %rdi, %rdi
    mov $XINIM_NATIVE_SYS_BRK, %rax
    syscall
    mov %rax, %r12

    lea 8192(%r12), %rdi
    mov $XINIM_NATIVE_SYS_BRK, %rax
    syscall
    cmp %rdi, %rax
    jne .failure

    movb $0x5a, (%r12)
    movb $0xa5, 4096(%r12)

    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_BRK, %rax
    syscall
    cmp %r12, %rax
    jne .failure

    mov $1, %rdi
    lea success_message(%rip), %rsi
    mov $success_message_size, %rdx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    xor %rdi, %rdi
    jmp .exit

.failure:
    mov $2, %rdi
    lea failure_message(%rip), %rsi
    mov $failure_message_size, %rdx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    mov $1, %rdi

.exit:
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall
    ud2

.size _start, . - _start

.section .rodata
success_message:
    .ascii "BRK_MAP_SHRINK_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "BRK_MAP_SHRINK_FAIL\n"
.set failure_message_size, . - failure_message

.section .note.GNU-stack,"",@progbits
