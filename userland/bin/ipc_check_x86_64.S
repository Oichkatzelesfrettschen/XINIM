#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

/*
 * Exercise the finite bootfs pipe descriptor pool from Ring 3.  The
 * descriptor table has room for fourteen complete backend pipe pairs; the
 * fifteenth allocation must fail without leaking its first descriptor.
 */
_start:
    lea pipe_descriptors(%rip), %r12
    xor %r13d, %r13d

.create_pipe:
    cmp $14, %r13d
    jae .check_exhaustion
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_PIPE, %rax
    syscall
    test %rax, %rax
    js .failure
    add $8, %r12
    inc %r13d
    jmp .create_pipe

.check_exhaustion:
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_PIPE, %rax
    syscall
    test %rax, %rax
    jns .failure

    lea pipe_descriptors(%rip), %r12
    xor %r13d, %r13d

.close_pipe:
    cmp $14, %r13d
    jae .success
    mov (%r12), %edi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    js .failure
    mov 4(%r12), %edi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    js .failure
    add $8, %r12
    inc %r13d
    jmp .close_pipe

.success:
    mov $1, %edi
    lea success_message(%rip), %rsi
    mov $success_message_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    xor %edi, %edi
    jmp .exit

.failure:
    mov $2, %edi
    lea failure_message(%rip), %rsi
    mov $failure_message_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    mov $1, %edi

.exit:
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall
    ud2

.size _start, . - _start

.section .rodata
success_message:
    .ascii "IPC_PIPE_EXHAUSTION_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "IPC_PIPE_EXHAUSTION_FAIL\n"
.set failure_message_size, . - failure_message

.section .bss
.balign 8
pipe_descriptors:
    .skip 112

.section .note.GNU-stack,"",@progbits
