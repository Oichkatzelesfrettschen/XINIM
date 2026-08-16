#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    mov $XINIM_NATIVE_SYS_GETPID, %rax
    syscall
    mov %rax, %r15
    xor %edi, %edi
    xor %esi, %esi
    mov $XINIM_NATIVE_SYS_SETPGID, %rax
    syscall
    test %rax, %rax
    js .failure

    lea usr1_action(%rip), %rsi
    xor %edx, %edx
    mov $10, %edi
    mov $XINIM_NATIVE_SYS_SIGACTION, %rax
    syscall
    test %rax, %rax
    js .failure
    lea usr2_action(%rip), %rsi
    xor %edx, %edx
    mov $12, %edi
    mov $XINIM_NATIVE_SYS_SIGACTION, %rax
    syscall
    test %rax, %rax
    js .failure

    mov %r15, %rdi
    mov $10, %esi
    mov $XINIM_NATIVE_SYS_KILL, %rax
    syscall
    test %rax, %rax
    js .failure
    cmpl $1, usr1_count(%rip)
    jne .failure
    lea signal_context_ok(%rip), %rsi
    mov $signal_context_ok_size, %edx
    call write_message

    xor %edi, %edi
    lea usr2_mask(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    mov $XINIM_NATIVE_SYS_RT_SIGPROCMASK, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %r15, %rdi
    mov $12, %esi
    mov $XINIM_NATIVE_SYS_KILL, %rax
    syscall
    test %rax, %rax
    js .failure
    cmpl $0, usr2_count(%rip)
    jne .failure
    mov $1, %edi
    lea usr2_mask(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    mov $XINIM_NATIVE_SYS_RT_SIGPROCMASK, %rax
    syscall
    test %rax, %rax
    js .failure
    cmpl $1, usr2_count(%rip)
    jne .failure
    lea signal_mask_ok(%rip), %rsi
    mov $signal_mask_ok_size, %edx
    call write_message

    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .signal_child
    mov %rax, %r14
    mov %r14, %rdi
    lea wait_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r14, %rax
    jne .failure
    mov wait_status(%rip), %eax
    and $0x7f, %eax
    cmp $15, %eax
    jne .failure
    lea signal_exit_ok(%rip), %rsi
    mov $signal_exit_ok_size, %edx
    call write_message

    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .job_child
    mov %rax, %r13
    mov %r13, %rdi
    mov %r13, %rsi
    mov $XINIM_NATIVE_SYS_SETPGID, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %r13, %rdi
    lea wait_status(%rip), %rsi
    mov $2, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r13, %rax
    jne .failure
    mov wait_status(%rip), %eax
    and $0xff, %eax
    cmp $0x7f, %eax
    jne .failure
    mov wait_status(%rip), %eax
    shr $8, %eax
    and $0xff, %eax
    cmp $19, %eax
    jne .failure

    mov %r13, %rdi
    neg %rdi
    mov $18, %esi
    mov $XINIM_NATIVE_SYS_KILL, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %r13, %rdi
    lea wait_status(%rip), %rsi
    mov $8, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r13, %rax
    jne .failure
    cmpl $0xffff, wait_status(%rip)
    jne .failure

    mov %r13, %rdi
    mov $15, %esi
    mov $XINIM_NATIVE_SYS_KILL, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %r13, %rdi
    lea wait_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r13, %rax
    jne .failure
    mov wait_status(%rip), %eax
    and $0x7f, %eax
    cmp $15, %eax
    jne .failure
    lea job_control_ok(%rip), %rsi
    mov $job_control_ok_size, %edx
    call write_message

    xor %edi, %edi
    mov $0x540f, %esi
    lea foreground_pgid(%rip), %rdx
    mov $XINIM_NATIVE_SYS_IOCTL, %rax
    syscall
    test %rax, %rax
    js .failure
    cmpl $1, foreground_pgid(%rip)
    jne .failure
    lea tty_pgrp_ok(%rip), %rsi
    mov $tty_pgrp_ok_size, %edx
    call write_message
    xor %edi, %edi
    jmp .exit

.signal_child:
    mov $XINIM_NATIVE_SYS_GETPID, %rax
    syscall
    mov %rax, %rdi
    mov $15, %esi
    mov $XINIM_NATIVE_SYS_KILL, %rax
    syscall
    jmp .failure

.job_child:
    xor %edi, %edi
    xor %esi, %esi
    mov $XINIM_NATIVE_SYS_SETPGID, %rax
    syscall
    test %rax, %rax
    js .failure
    mov $XINIM_NATIVE_SYS_GETPID, %rax
    syscall
    mov %rax, %rdi
    mov $19, %esi
    mov $XINIM_NATIVE_SYS_KILL, %rax
    syscall
.job_spin:
    jmp .job_spin

.failure:
    lea failure_message(%rip), %rsi
    mov $failure_message_size, %edx
    call write_message
    mov $1, %edi

.exit:
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall
    ud2

.type write_message, @function
write_message:
    mov $1, %edi
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    ret
.size write_message, . - write_message

.type usr1_handler, @function
usr1_handler:
    incl usr1_count(%rip)
    ret
.size usr1_handler, . - usr1_handler

.type usr2_handler, @function
usr2_handler:
    incl usr2_count(%rip)
    ret
.size usr2_handler, . - usr2_handler

.size _start, . - _start

.section .rodata
signal_context_ok:
    .ascii "SIGNAL_CONTEXT_OK\n"
.set signal_context_ok_size, . - signal_context_ok
signal_mask_ok:
    .ascii "SIGNAL_MASK_OK\n"
.set signal_mask_ok_size, . - signal_mask_ok
signal_exit_ok:
    .ascii "SIGNAL_EXIT_STATUS_OK\n"
.set signal_exit_ok_size, . - signal_exit_ok
job_control_ok:
    .ascii "JOB_CONTROL_OK\n"
.set job_control_ok_size, . - job_control_ok
tty_pgrp_ok:
    .ascii "TTY_PGRP_OK\n"
.set tty_pgrp_ok_size, . - tty_pgrp_ok
failure_message:
    .ascii "SIGNAL_JOB_CHECK_FAIL\n"
.set failure_message_size, . - failure_message

.section .data
.align 8
usr1_action:
    .quad usr1_handler
    .quad 0
    .quad 0
    .quad 0
usr2_action:
    .quad usr2_handler
    .quad 0
    .quad 0
    .quad 0
usr2_mask:
    .quad 1 << 12

.section .bss
.align 8
usr1_count:
    .long 0
usr2_count:
    .long 0
wait_status:
    .long 0
foreground_pgid:
    .long 0

.section .note.GNU-stack,"",@progbits
