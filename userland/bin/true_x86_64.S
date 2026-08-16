#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    xor %rdi, %rdi
    mov $XINIM_NATIVE_SYS_EXIT, %rax
    syscall
    ud2

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
