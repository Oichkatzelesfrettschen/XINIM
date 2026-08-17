#include <xinim/abi/native_syscall_numbers.h>

.set AF_UNIX, 1
.set AF_INET, 2
.set SOCK_DGRAM, 2
.set F_SETFD, 2
.set FD_CLOEXEC, 1

.section .text
.global _start
.type _start, @function

_start:
    mov (%rsp), %eax
    cmp $2, %eax
    je .check_exec_entry
    jmp .normal_entry

.check_exec_entry:
    mov 16(%rsp), %rdi
    lea after_exec_argument(%rip), %rsi
    call strings_equal
    test %eax, %eax
    jz .normal_entry

    mov $3, %edi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    cmp $-9, %rax
    jne .failure
    mov $1, %edi
    lea exec_success_message(%rip), %rsi
    mov $exec_success_message_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    xor %edi, %edi
    jmp .exit

.normal_entry:
    mov $AF_UNIX, %edi
    mov $SOCK_DGRAM, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_SOCKET, %rax
    syscall
    cmp $-97, %rax
    jne .failure

    mov $AF_INET, %edi
    mov $SOCK_DGRAM, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_SOCKET, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r12

    mov $AF_INET, %edi
    mov $SOCK_DGRAM, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_SOCKET, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r13

    mov %r12, %rdi
    lea address_a(%rip), %rsi
    mov $16, %edx
    mov $XINIM_NATIVE_SYS_BIND, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r13, %rdi
    lea address_b(%rip), %rsi
    mov $16, %edx
    mov $XINIM_NATIVE_SYS_BIND, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r12, %rdi
    lea address_a_observed(%rip), %rsi
    lea address_length(%rip), %rdx
    movq $16, address_length(%rip)
    mov $XINIM_NATIVE_SYS_GETSOCKNAME, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpw $2, address_a_observed(%rip)
    jne .failure
    cmpw $0x29a0, 2+address_a_observed(%rip)
    jne .failure

    mov %r12, %rdi
    lea first_payload(%rip), %rsi
    mov $5, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    cmp $-89, %rax
    jne .failure

    mov %r12, %rdi
    lea address_b(%rip), %rsi
    mov $16, %edx
    mov $XINIM_NATIVE_SYS_CONNECT, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r12, %rdi
    lea address_a_observed(%rip), %rsi
    lea address_length(%rip), %rdx
    movq $16, address_length(%rip)
    mov $XINIM_NATIVE_SYS_GETPEERNAME, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpw $2, address_a_observed(%rip)
    jne .failure
    cmpw $0x2aa0, 2+address_a_observed(%rip)
    jne .failure
    lea read_set(%rip), %rdi
    mov %r13, %rsi
    call set_single_descriptor
    movq $0, zero_timeout(%rip)
    movq $0, 8+zero_timeout(%rip)
    lea 1(%r13), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    lea zero_timeout(%rip), %r8
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r12, %rdi
    mov %r12, %rsi
    mov $XINIM_NATIVE_SYS_DUP, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r14
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    mov %r14, %rdi
    lea first_payload(%rip), %rsi
    mov $5, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    cmp $5, %rax
    jne .failure

    lea read_set(%rip), %rdi
    mov %r13, %rsi
    call set_single_descriptor
    movq $1, event_timeout(%rip)
    movq $0, 8+event_timeout(%rip)
    lea 1(%r13), %rdi
    lea read_set(%rip), %rsi
    xor %edx, %edx
    xor %ecx, %ecx
    lea event_timeout(%rip), %r8
    mov $XINIM_NATIVE_SYS_SELECT, %rax
    syscall
    cmp $1, %rax
    jne .failure
    lea read_set(%rip), %rdi
    mov %r13, %rsi
    call descriptor_is_set
    cmp $1, %rax
    jne .failure

    mov %r13, %rdi
    lea receive_buffer(%rip), %rsi
    mov $64, %edx
    xor %r10d, %r10d
    lea source_address(%rip), %r8
    lea source_length(%rip), %r9
    movq $16, source_length(%rip)
    mov $XINIM_NATIVE_SYS_RECVFROM, %rax
    syscall
    cmp $5, %rax
    jne .failure
    lea receive_buffer(%rip), %rdi
    lea first_payload(%rip), %rsi
    mov $5, %edx
    call bytes_equal
    test %eax, %eax
    jz .failure
    cmpw $0x29a0, 2+source_address(%rip)
    jne .failure

    mov %r14, %rdi
    mov $3, %esi
    mov $XINIM_NATIVE_SYS_DUP2, %rax
    syscall
    cmp $3, %rax
    jne .failure
    mov %r14, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov $3, %r12d

    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .exec_child
    mov %rax, %r15
    mov %r15, %rdi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r15, %rax
    jne .failure

    mov %r12, %rdi
    mov $1, %rsi
    mov $XINIM_NATIVE_SYS_SHUTDOWN, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov %r12, %rdi
    lea second_payload(%rip), %rsi
    mov $6, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    cmp $-108, %rax
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
    mov %r13, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    cmp $-9, %rax
    jne .failure

    mov $1, %edi
    lea success_message(%rip), %rsi
    mov $success_message_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    xor %edi, %edi
    jmp .exit

.exec_child:
    mov %r12, %rdi
    mov $F_SETFD, %esi
    mov $FD_CLOEXEC, %edx
    mov $XINIM_NATIVE_SYS_FCNTL, %rax
    syscall
    test %rax, %rax
    jne .child_failure
    lea socket_check_path(%rip), %rdi
    lea exec_argv(%rip), %rsi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_EXECVE, %rax
    syscall

.child_failure:
    mov $2, %edi

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

.type bytes_equal, @function
bytes_equal:
    test %rdx, %rdx
    jz .bytes_equal_true
    mov %rdx, %rcx
    repe cmpsb
    sete %al
    movzbl %al, %eax
    ret
.bytes_equal_true:
    mov $1, %eax
    ret
.size bytes_equal, . - bytes_equal

.type strings_equal, @function
strings_equal:
.strings_equal_loop:
    movzbq (%rdi), %rax
    movzbq (%rsi), %rdx
    cmp %dl, %al
    jne .strings_equal_false
    test %al, %al
    je .strings_equal_true
    inc %rdi
    inc %rsi
    jmp .strings_equal_loop
.strings_equal_true:
    mov $1, %eax
    ret
.strings_equal_false:
    xor %eax, %eax
    ret
.size strings_equal, . - strings_equal

.section .rodata
address_a:
    .short 2, 0x29a0
    .long 0x0100007f
    .zero 8
address_b:
    .short 2, 0x2aa0
    .long 0x0100007f
    .zero 8
first_payload:
    .ascii "RING3"
second_payload:
    .ascii "SECOND"
socket_check_path:
    .asciz "/bin/socket-check"
after_exec_argument:
    .asciz "after-exec"
exec_argv:
    .quad socket_check_path, after_exec_argument, 0
success_message:
    .ascii "SOCKET_Q35_RING3_LIFECYCLE_OK\n"
.set success_message_size, . - success_message
exec_success_message:
    .ascii "SOCKET_EXEC_CLOEXEC_OK\n"
.set exec_success_message_size, . - exec_success_message
failure_message:
    .ascii "SOCKET_Q35_RING3_LIFECYCLE_FAIL\n"
.set failure_message_size, . - failure_message

.section .bss
.balign 16
read_set:
    .skip 128
zero_timeout:
    .skip 16
event_timeout:
    .skip 16
address_a_observed:
    .skip 16
source_address:
    .skip 16
address_length:
    .skip 8
source_length:
    .skip 8
receive_buffer:
    .skip 64

.section .note.GNU-stack,"",@progbits
