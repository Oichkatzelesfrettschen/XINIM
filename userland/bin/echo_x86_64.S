#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    mov (%rsp), %r12
    lea 8(%rsp), %r13
    mov $1, %r14

.argument_loop:
    cmp %r12, %r14
    jae .newline
    cmp $1, %r14
    je .write_argument
    mov $6, %rax
    mov $1, %rdi
    lea space(%rip), %rsi
    mov $1, %rdx
    syscall

.write_argument:
    mov (%r13,%r14,8), %rsi
    xor %rdx, %rdx
.length_loop:
    cmpb $0, (%rsi,%rdx)
    je .emit_argument
    inc %rdx
    jmp .length_loop
.emit_argument:
    mov $6, %rax
    mov $1, %rdi
    syscall
    inc %r14
    jmp .argument_loop

.newline:
    mov $6, %rax
    mov $1, %rdi
    lea newline(%rip), %rsi
    mov $1, %rdx
    syscall
    xor %rdi, %rdi
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall
    ud2

.size _start, . - _start

.section .rodata
space:
    .ascii " "
newline:
    .ascii "\n"

.section .note.GNU-stack,"",@progbits
