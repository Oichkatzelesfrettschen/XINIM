#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    lea realtime_value(%rip), %rdi
    xor %esi, %esi
    mov $XINIM_NATIVE_SYS_GETTIMEOFDAY, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpq $1700000000, realtime_value(%rip)
    jl .failure

    mov $1, %edi
    lea monotonic_before(%rip), %rsi
    mov $XINIM_NATIVE_SYS_CLOCK_GETTIME, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea sleep_request(%rip), %rdi
    lea sleep_remaining(%rip), %rsi
    mov $XINIM_NATIVE_SYS_NANOSLEEP, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov $1, %edi
    lea monotonic_after(%rip), %rsi
    mov $XINIM_NATIVE_SYS_CLOCK_GETTIME, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov monotonic_after(%rip), %rax
    cmp monotonic_before(%rip), %rax
    jl .failure
    jne .sleep_elapsed
    mov 8+monotonic_after(%rip), %rax
    sub 8+monotonic_before(%rip), %rax
    cmp $20000000, %rax
    jl .failure

.sleep_elapsed:
    mov $7, %edi
    lea resource_limit(%rip), %rsi
    mov $XINIM_NATIVE_SYS_GETRLIMIT, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpq $1024, resource_limit(%rip)
    jne .failure
    cmpq $1024, 8+resource_limit(%rip)
    jne .failure

    xor %edi, %edi
    lea resource_usage(%rip), %rsi
    mov $XINIM_NATIVE_SYS_GETRUSAGE, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov $0077, %edi
    mov $XINIM_NATIVE_SYS_UMASK, %rax
    syscall
    cmp $0022, %rax
    jne .failure
    mov $0022, %edi
    mov $XINIM_NATIVE_SYS_UMASK, %rax
    syscall
    cmp $0077, %rax
    jne .failure

    mov $14, %edi
    lea alarm_action(%rip), %rsi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_SIGACTION, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov $1, %edi
    mov $XINIM_NATIVE_SYS_ALARM, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea empty_signal_mask(%rip), %rdi
    mov $8, %esi
    mov $XINIM_NATIVE_SYS_SIGSUSPEND, %rax
    syscall
    cmp $-4, %rax
    jne .failure
    cmpl $1, alarm_count(%rip)
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

.type alarm_handler, @function
alarm_handler:
    incl alarm_count(%rip)
    ret
.size alarm_handler, . - alarm_handler

.size _start, . - _start

.section .rodata
success_message:
    .ascii "TIME_SLEEP_RESOURCE_SUSPEND_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "TIME_SLEEP_RESOURCE_SUSPEND_FAIL\n"
.set failure_message_size, . - failure_message
sleep_request:
    .quad 0
    .quad 20000000
alarm_action:
    .quad alarm_handler
    .quad 0
    .quad 0
    .quad 0
empty_signal_mask:
    .quad 0

.section .bss
.balign 16
realtime_value:
    .skip 16
monotonic_before:
    .skip 16
monotonic_after:
    .skip 16
sleep_remaining:
    .skip 16
resource_limit:
    .skip 16
resource_usage:
    .skip 144
alarm_count:
    .skip 4

.section .note.GNU-stack,"",@progbits
