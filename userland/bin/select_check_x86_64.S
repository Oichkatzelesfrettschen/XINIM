#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    lea regular_path(%rip), %rdi
    xor %esi, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r12
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call set_single_descriptor
    lea 1(%r12), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    xor %r8d, %r8d
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    cmp $1, %rax
    jne .failure
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call descriptor_is_set
    cmp $1, %rax
    jne .failure
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea pipe_descriptors(%rip), %rdi
    mov $XINIM_NATIVE_SYS_PIPE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov pipe_descriptors(%rip), %r12d
    mov 4+pipe_descriptors(%rip), %r13d

    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call set_single_descriptor
    movq $0, zero_timeout(%rip)
    movq $0, 8+zero_timeout(%rip)
    lea 1(%r12), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    lea zero_timeout(%rip), %r8
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    test %rax, %rax
    jne .failure
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call descriptor_is_set
    test %rax, %rax
    jne .failure

    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .writer_child
    mov %rax, %r14

    mov %r13, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call set_single_descriptor
    movq $1, event_timeout(%rip)
    movq $0, 8+event_timeout(%rip)
    lea 1(%r12), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    lea event_timeout(%rip), %r8
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    cmp $1, %rax
    jne .failure
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call descriptor_is_set
    cmp $1, %rax
    jne .failure
    mov %r12, %rdi
    lea pipe_byte(%rip), %rsi
    mov $1, %edx
    mov $XINIM_NATIVE_SYS_READ, %rax
    syscall
    cmp $1, %rax
    jne .failure
    cmpb $0x5a, pipe_byte(%rip)
    jne .failure
    mov %r14, %rdi
    xor %esi, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r14, %rax
    jne .failure
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea pipe_descriptors(%rip), %rdi
    mov $XINIM_NATIVE_SYS_PIPE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov pipe_descriptors(%rip), %r12d
    mov 4+pipe_descriptors(%rip), %r13d
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call set_single_descriptor
    movq $0, short_timeout(%rip)
    movq $50000, 8+short_timeout(%rip)
    lea 1(%r12), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    lea short_timeout(%rip), %r8
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpq $0, short_timeout(%rip)
    jne .failure
    cmpq $0, 8+short_timeout(%rip)
    jne .failure
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov %r13, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    lea pipe_descriptors(%rip), %rdi
    mov $XINIM_NATIVE_SYS_PIPE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov pipe_descriptors(%rip), %r12d
    mov 4+pipe_descriptors(%rip), %r13d
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
    lea read_set(%rip), %rdi
    mov %r12, %rsi
    call set_single_descriptor
    movq $2, interrupted_timeout(%rip)
    movq $0, 8+interrupted_timeout(%rip)
    lea 1(%r12), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    lea interrupted_timeout(%rip), %r8
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    cmp $-4, %rax
    jne .failure
    cmpl $1, alarm_count(%rip)
    jne .failure
    cmpq $2, interrupted_timeout(%rip)
    jge .failure
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov %r13, %rdi
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

.writer_child:
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    lea child_sleep(%rip), %rdi
    xor %esi, %esi
    mov $XINIM_NATIVE_SYS_NANOSLEEP, %rax
    syscall
    test %rax, %rax
    jne .child_failure
    mov %r13, %rdi
    lea child_byte(%rip), %rsi
    mov $1, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    cmp $1, %rax
    jne .child_failure
    mov %r13, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    xor %edi, %edi
    jmp .exit

.child_failure:
    mov $2, %edi
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

.type set_single_descriptor, @function
set_single_descriptor:
    mov %rdi, %rdx
    xor %eax, %eax
    mov $16, %ecx
    rep stosq
    mov %rsi, %rax
    shr $6, %rax
    lea (%rdx,%rax,8), %rdx
    mov %rsi, %rcx
    and $63, %ecx
    mov $1, %rax
    shl %cl, %rax
    or %rax, (%rdx)
    ret
.size set_single_descriptor, . - set_single_descriptor

.type descriptor_is_set, @function
descriptor_is_set:
    mov %rsi, %rax
    shr $6, %rax
    mov (%rdi,%rax,8), %rax
    mov %rsi, %rcx
    and $63, %ecx
    shr %cl, %rax
    and $1, %eax
    ret
.size descriptor_is_set, . - descriptor_is_set

.type alarm_handler, @function
alarm_handler:
    incl alarm_count(%rip)
    ret
.size alarm_handler, . - alarm_handler

.size _start, . - _start

.section .rodata
regular_path:
    .asciz "/etc/motd"
child_sleep:
    .quad 0, 20000000
child_byte:
    .byte 0x5a
alarm_action:
    .quad alarm_handler, 0, 0, 0
success_message:
    .ascii "SELECT_EVENT_TIMEOUT_EINTR_OK\n"
.set success_message_size, . - success_message
failure_message:
    .ascii "SELECT_EVENT_TIMEOUT_EINTR_FAIL\n"
.set failure_message_size, . - failure_message

.section .bss
.balign 16
pipe_descriptors:
    .skip 8
read_set:
    .skip 128
zero_timeout:
    .skip 16
event_timeout:
    .skip 16
short_timeout:
    .skip 16
interrupted_timeout:
    .skip 16
pipe_byte:
    .skip 1
alarm_count:
    .skip 4

.section .note.GNU-stack,"",@progbits
