#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    lea bin_path(%rip), %rdi
    mov $0x10000, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r12

    mov %r12, %rdi
    mov $2, %esi
    mov $1, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r12, %rdi
    mov $1, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    cmp $1, %rax
    jne .failure

    mov %r12, %rdi
    mov $3, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    test $0x10000, %eax
    jz .failure

    mov %r12, %rdi
    mov $0, %esi
    mov $10, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    cmp $10, %rax
    jl .failure
    mov %rax, %r13

    mov %r13, %rdi
    mov $1, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea mksh_path(%rip), %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_STAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov 24+stat_buffer(%rip), %eax
    and $0xf000, %eax
    cmp $0x8000, %eax
    jne .failure
    cmpq $0, 48+stat_buffer(%rip)
    jle .failure

    mov %r12, %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_FSTAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov 24+stat_buffer(%rip), %eax
    and $0xf000, %eax
    cmp $0x4000, %eax
    jne .failure

    mov %r12, %rdi
    lea directory_buffer(%rip), %rsi
    mov $1024, %edx
    mov $XINIM_NATIVE_SYS_GETDENTS, %rax
    syscall
    test %rax, %rax
    jle .failure
    mov %rax, %r14
    lea directory_buffer(%rip), %rsi
    xor %ecx, %ecx

.scan_directory:
    lea 4(%rcx), %rax
    cmp %r14, %rax
    ja .failure
    cmpl $0x68736b6d, (%rsi,%rcx)
    je .found_mksh
    inc %rcx
    jmp .scan_directory

.found_mksh:
    mov %r13, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
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
bin_path:
    .asciz "/bin"
mksh_path:
    .asciz "/bin/mksh"
success_message:
    .ascii "FS_ABI_FCNTL_DIRENT_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "FS_ABI_FCNTL_DIRENT_FAIL\n"
.set failure_message_size, . - failure_message

.section .bss
.balign 16
stat_buffer:
    .skip 144
.balign 16
directory_buffer:
    .skip 1024

.section .note.GNU-stack,"",@progbits
