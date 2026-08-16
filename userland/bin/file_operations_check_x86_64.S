#include <xinim/abi/native_syscall_numbers.h>

.set O_RDONLY, 0x0000
.set O_RDWR, 0x0002
.set O_CREAT, 0x0040
.set O_TRUNC, 0x0200
.set LOCK_EX, 0x0002
.set LOCK_NB, 0x0004
.set LOCK_UN, 0x0008

.section .text
.global _start
.type _start, @function

_start:
    movb $'A', failure_stage(%rip)
    lea file_path(%rip), %rdi
    mov $(O_RDWR | O_CREAT | O_TRUNC), %esi
    mov $0600, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r12

    movb $'B', failure_stage(%rip)
    mov %r12, %rdi
    lea file_contents(%rip), %rsi
    mov $file_contents_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    cmp $file_contents_size, %rax
    jne .failure

    movb $'C', failure_stage(%rip)
    mov %r12, %rdi
    mov $3, %esi
    mov $XINIM_NATIVE_SYS_FTRUNCATE, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'D', failure_stage(%rip)
    mov %r12, %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_FSTAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpq $3, 48+stat_buffer(%rip)
    jne .failure

    movb $'E', failure_stage(%rip)
    lea file_path(%rip), %rdi
    mov $0x12345678, %esi
    mov $0x23456789, %edx
    mov $XINIM_NATIVE_SYS_CHOWN, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'F', failure_stage(%rip)
    lea file_path(%rip), %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_STAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpl $0x12345678, 28+stat_buffer(%rip)
    jne .failure
    cmpl $0x23456789, 32+stat_buffer(%rip)
    jne .failure

    movb $'G', failure_stage(%rip)
    mov %r12, %rdi
    mov $0xffffffff, %esi
    mov $77, %edx
    mov $XINIM_NATIVE_SYS_FCHOWN, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'H', failure_stage(%rip)
    mov %r12, %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_FSTAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpl $0x12345678, 28+stat_buffer(%rip)
    jne .failure
    cmpl $77, 32+stat_buffer(%rip)
    jne .failure

    movb $'I', failure_stage(%rip)
    mov %r12, %rdi
    mov $LOCK_EX, %esi
    mov $XINIM_NATIVE_SYS_FLOCK, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'J', failure_stage(%rip)
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_DUP, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r13

    movb $'K', failure_stage(%rip)
    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .child
    mov %rax, %r14

    movb $'L', failure_stage(%rip)
    mov %r14, %rdi
    lea wait_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r14, %rax
    jne .failure
    cmpl $0, wait_status(%rip)
    jne .failure

    movb $'M', failure_stage(%rip)
    lea file_path(%rip), %rdi
    mov $O_RDWR, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r14

    movb $'N', failure_stage(%rip)
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'O', failure_stage(%rip)
    mov %r14, %rdi
    mov $(LOCK_EX | LOCK_NB), %esi
    mov $XINIM_NATIVE_SYS_FLOCK, %rax
    syscall
    cmp $-11, %rax
    jne .failure

    movb $'P', failure_stage(%rip)
    mov %r13, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'Q', failure_stage(%rip)
    mov %r14, %rdi
    mov $(LOCK_EX | LOCK_NB), %esi
    mov $XINIM_NATIVE_SYS_FLOCK, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'R', failure_stage(%rip)
    mov %r14, %rdi
    mov $LOCK_UN, %esi
    mov $XINIM_NATIVE_SYS_FLOCK, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'S', failure_stage(%rip)
    lea file_path(%rip), %rdi
    mov $1, %esi
    mov $XINIM_NATIVE_SYS_TRUNCATE, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'T', failure_stage(%rip)
    mov %r14, %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_FSTAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    cmpq $1, 48+stat_buffer(%rip)
    jne .failure

    movb $'U', failure_stage(%rip)
    lea file_path(%rip), %rdi
    mov $O_RDONLY, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r15

    movb $'V', failure_stage(%rip)
    mov %r15, %rdi
    xor %esi, %esi
    mov $XINIM_NATIVE_SYS_FTRUNCATE, %rax
    syscall
    cmp $-9, %rax
    jne .failure

    movb $'W', failure_stage(%rip)
    mov %r15, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov %r14, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'a', failure_stage(%rip)
    lea absolute_target(%rip), %rdi
    lea absolute_link(%rip), %rsi
    mov $XINIM_NATIVE_SYS_SYMLINK, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'b', failure_stage(%rip)
    lea absolute_link(%rip), %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_LSTAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov 24+stat_buffer(%rip), %eax
    and $0xf000, %eax
    cmp $0xa000, %eax
    jne .failure
    cmpq $absolute_target_size, 48+stat_buffer(%rip)
    jne .failure

    movb $'c', failure_stage(%rip)
    lea absolute_link(%rip), %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_STAT, %rax
    syscall
    test %rax, %rax
    jne .failure
    mov 24+stat_buffer(%rip), %eax
    and $0xf000, %eax
    cmp $0x8000, %eax
    jne .failure
    cmpq $1, 48+stat_buffer(%rip)
    jne .failure

    movb $'d', failure_stage(%rip)
    movb $0x5a, 4+link_buffer(%rip)
    lea absolute_link(%rip), %rdi
    lea link_buffer(%rip), %rsi
    mov $4, %edx
    mov $XINIM_NATIVE_SYS_READLINK, %rax
    syscall
    cmp $4, %rax
    jne .failure
    cmpl $0x706d742f, link_buffer(%rip)
    jne .failure
    cmpb $0x5a, 4+link_buffer(%rip)
    jne .failure

    movb $'e', failure_stage(%rip)
    lea relative_target(%rip), %rdi
    lea relative_link(%rip), %rsi
    mov $XINIM_NATIVE_SYS_SYMLINK, %rax
    syscall
    test %rax, %rax
    jne .failure
    lea relative_link(%rip), %rdi
    mov $O_RDONLY, %esi
    xor %edx, %edx
    mov $XINIM_NATIVE_SYS_OPEN, %rax
    syscall
    test %rax, %rax
    js .failure
    mov %rax, %r12
    mov %r12, %rdi
    lea link_buffer(%rip), %rsi
    mov $1, %edx
    mov $XINIM_NATIVE_SYS_READ, %rax
    syscall
    cmp $1, %rax
    jne .failure
    cmpb $'a', link_buffer(%rip)
    jne .failure
    mov %r12, %rdi
    mov $XINIM_NATIVE_SYS_CLOSE, %rax
    syscall
    test %rax, %rax
    jne .failure

    movb $'f', failure_stage(%rip)
    lea bin_target(%rip), %rdi
    lea bin_link(%rip), %rsi
    mov $XINIM_NATIVE_SYS_SYMLINK, %rax
    syscall
    test %rax, %rax
    jne .failure
    movb $'i', failure_stage(%rip)
    mov $XINIM_NATIVE_SYS_FORK, %rax
    syscall
    test %rax, %rax
    js .failure
    jz .exec_child
    mov %rax, %r12
    movb $'j', failure_stage(%rip)
    mov %r12, %rdi
    lea wait_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $XINIM_NATIVE_SYS_WAIT4, %rax
    syscall
    cmp %r12, %rax
    jne .failure
    movb $'k', failure_stage(%rip)
    cmpl $0, wait_status(%rip)
    jne .failure

    movb $'g', failure_stage(%rip)
    lea loop_b_target(%rip), %rdi
    lea loop_a_path(%rip), %rsi
    mov $XINIM_NATIVE_SYS_SYMLINK, %rax
    syscall
    test %rax, %rax
    jne .failure
    lea loop_a_target(%rip), %rdi
    lea loop_b_path(%rip), %rsi
    mov $XINIM_NATIVE_SYS_SYMLINK, %rax
    syscall
    test %rax, %rax
    jne .failure
    lea loop_a_path(%rip), %rdi
    lea stat_buffer(%rip), %rsi
    mov $XINIM_NATIVE_SYS_STAT, %rax
    syscall
    cmp $-2, %rax
    jne .failure

    movb $'h', failure_stage(%rip)
    lea file_path(%rip), %rdi
    lea link_buffer(%rip), %rsi
    mov $8, %edx
    mov $XINIM_NATIVE_SYS_READLINK, %rax
    syscall
    cmp $-22, %rax
    jne .failure

    mov $1, %edi
    lea success_message(%rip), %rsi
    mov $success_message_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    xor %edi, %edi
    jmp .exit

.child:
    movb $'X', failure_stage(%rip)
    mov %r13, %rdi
    mov $LOCK_EX, %esi
    mov $XINIM_NATIVE_SYS_FLOCK, %rax
    syscall
    test %rax, %rax
    jne .child_failure
    xor %edi, %edi
    jmp .exit

.child_failure:
    mov $1, %edi
    jmp .exit

.exec_child:
    lea exec_path(%rip), %rax
    mov %rax, exec_arguments(%rip)
    lea exec_path(%rip), %rdi
    lea exec_arguments(%rip), %rsi
    lea exec_environment(%rip), %rdx
    mov $XINIM_NATIVE_SYS_EXECVE, %rax
    syscall
    mov $1, %edi
    jmp .exit

.failure:
    mov $2, %edi
    lea failure_prefix(%rip), %rsi
    mov $failure_prefix_size, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    mov $2, %edi
    lea failure_stage(%rip), %rsi
    mov $1, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    mov $2, %edi
    lea newline(%rip), %rsi
    mov $1, %edx
    mov $XINIM_NATIVE_SYS_WRITE, %rax
    syscall
    mov $1, %edi

.exit:
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall
    ud2

.size _start, . - _start

.section .rodata
file_path:
    .asciz "/tmp/file-operations-check"
file_contents:
    .ascii "abcdefgh"
.set file_contents_size, . - file_contents
success_message:
    .ascii "FILE_TRUNCATE_CHOWN_FLOCK_SYMLINK_OK\n"
.set success_message_size, . - success_message
failure_prefix:
    .ascii "FILE_TRUNCATE_CHOWN_FLOCK_FAIL stage="
.set failure_prefix_size, . - failure_prefix
newline:
    .ascii "\n"
absolute_target:
    .ascii "/tmp/file-operations-check"
.set absolute_target_size, . - absolute_target
    .byte 0
absolute_link:
    .asciz "/tmp/absolute-link-check"
relative_target:
    .asciz "file-operations-check"
relative_link:
    .asciz "/tmp/relative-link-check"
bin_target:
    .asciz "/bin"
bin_link:
    .asciz "/tmp/bin-link-check"
exec_path:
    .asciz "/tmp/bin-link-check/true"
loop_a_target:
    .asciz "loop-a-check"
loop_b_target:
    .asciz "loop-b-check"
loop_a_path:
    .asciz "/tmp/loop-a-check"
loop_b_path:
    .asciz "/tmp/loop-b-check"

.section .bss
.balign 16
stat_buffer:
    .skip 144
wait_status:
    .skip 4
failure_stage:
    .skip 1
link_buffer:
    .skip 32
.balign 8
exec_arguments:
    .skip 16
exec_environment:
    .skip 8

.section .note.GNU-stack,"",@progbits
