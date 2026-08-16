#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .child

    mov %rax, %r12
    rdtsc
    shl $32, %rdx
    or %rax, %rdx
    lea 2000000000(%rdx), %r13
.parent_spin:
    rdtsc
    shl $32, %rdx
    or %rax, %rdx
    cmp %r13, %rdx
    jb .parent_spin

    mov $1, %rdi
    lea parent_message(%rip), %rsi
    mov $parent_message_size, %rdx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall

    mov %r12, %rdi
    lea wait_status(%rip), %rsi
    xor %rdx, %rdx
    xor %r10, %r10
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r12, %rax
    jne .failure
    xor %rdi, %rdi
    jmp .exit

.child:
    mov $1, %rdi
    lea child_message(%rip), %rsi
    mov $child_message_size, %rdx
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
child_message:
    .ascii "PREEMPT_CHILD_RAN\n"
.set child_message_size, . - child_message
parent_message:
    .ascii "PREEMPT_PARENT_DONE\n"
.set parent_message_size, . - parent_message
failure_message:
    .ascii "PREEMPT_CHECK_FAIL\n"
.set failure_message_size, . - failure_message

.section .bss
.align 4
wait_status:
    .long 0

.section .note.GNU-stack,"",@progbits
