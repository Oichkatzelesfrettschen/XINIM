#include <xinim/abi/native_syscall_numbers.h>

.set TCGETS, 0x5401
.set TCFLSH, 0x540b
.set TIOCGWINSZ, 0x5413
.set TIOCSWINSZ, 0x5414

.section .text
.global _start
.type _start, @function

_start:
    xor %edi, %edi
    xor %esi, %esi
    mov $10, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    cmp $10, %rax
    jl .failure
    mov %rax, %r12

    mov %r12, %rdi
    mov $TCGETS, %esi
    lea terminal_attributes(%rip), %rdx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r12, %rdi
    mov $TIOCGWINSZ, %esi
    lea original_window(%rip), %rdx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpw $24, original_window(%rip)
    jne .failure
    cmpw $80, 2+original_window(%rip)
    jne .failure

    mov %r12, %rdi
    mov $TIOCSWINSZ, %esi
    lea requested_window(%rip), %rdx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r12, %rdi
    mov $TIOCGWINSZ, %esi
    lea observed_window(%rip), %rdx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpw $30, observed_window(%rip)
    jne .failure
    cmpw $100, 2+observed_window(%rip)
    jne .failure

    mov %r12, %rdi
    mov $TCFLSH, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea regular_path(%rip), %rdi
    xor %esi, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r13
    mov %r13, %rdi
    mov $TCGETS, %esi
    lea terminal_attributes(%rip), %rdx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    cmp $-25, %rax
    jne .failure

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
regular_path:
    .asciz "/etc/motd"
requested_window:
    .short 30, 100, 0, 0
success_message:
    .ascii "TTY_DUP_FLUSH_WINDOW_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "TTY_DUP_FLUSH_WINDOW_FAIL\n"
.set failure_message_size, . - failure_message

.section .bss
.balign 16
terminal_attributes:
    .skip 36
original_window:
    .skip 8
observed_window:
    .skip 8

.section .note.GNU-stack,"",@progbits
