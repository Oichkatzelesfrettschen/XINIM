#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    xor %edi, %edi
    mov $8192, %esi
    mov $3, %edx
    mov $0x22, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    mov $XINIM_NATIVE_SYS_MMAP, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r12

    cmpb $0, (%r12)
    jne .failure
    cmpb $0, 4096(%r12)
    jne .failure
    movabs $0x1122334455667788, %rax
    mov %rax, (%r12)
    movabs $0x8877665544332211, %rax
    mov %rax, 4096(%r12)

    mov %r12, %rdi
    mov $8192, %esi
    mov $8192, %edx
    mov $3, %r10d
    mov %r12, %r8
    mov $XINIM_NATIVE_SYS_MREMAP, %rax
    syscall
    cmp $-22, %rax
    jne .failure

    mov %r12, %rdi
    mov $8192, %esi
    mov $8192, %edx
    mov $1, %r10d
    movabs $0x5a5a5a5a5a5a5a5a, %r8
    mov $XINIM_NATIVE_SYS_MREMAP, %rax
    syscall
    cmp %r12, %rax
    jne .failure

    mov %r12, %rdi
    mov $8192, %esi
    mov $12288, %edx
    mov $1, %r10d
    xor %r8d, %r8d
    mov $XINIM_NATIVE_SYS_MREMAP, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r13

    movabs $0x1122334455667788, %rax
    cmp %rax, (%r13)
    jne .failure
    movabs $0x8877665544332211, %rax
    cmp %rax, 4096(%r13)
    jne .failure
    cmpb $0, 8192(%r13)
    jne .failure
    movb $0x5a, 8192(%r13)

    mov %r13, %rdi
    mov $12288, %esi
    mov $4096, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    mov $XINIM_NATIVE_SYS_MREMAP, %rax
    syscall
    cmp %r13, %rax
    jne .failure
    movabs $0x1122334455667788, %rax
    cmp %rax, (%r13)
    jne .failure

    mov %r13, %rdi
    mov $4096, %esi
    mov $XINIM_NATIVE_SYS_MUNMAP, %rax
    syscall
    test %rax, %rax
    jne .failure

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
    .ascii "MMAP_REMAP_UNMAP_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "MMAP_REMAP_UNMAP_FAIL\n"
.set failure_message_size, . - failure_message

.section .note.GNU-stack,"",@progbits
