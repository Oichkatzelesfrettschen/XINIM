#include <xinim/abi/native_syscall_numbers.h>

.section .text
.global _start
.type _start, @function

_start:
    mov %esp, %ebp
    mov (%ebp), %eax
    lea 4(%ebp), %edx
    lea 4(%edx, %eax, 4), %ecx

    # The loader guarantees only word alignment. Establish the i386 SysV
    # 16-byte pre-call alignment while retaining the original process stack.
    and $-16, %esp
    sub $4, %esp
    push %ecx
    push %edx
    push %eax
    call xinim_user_main
    add $16, %esp
    mov %eax, %ebx
    mov $XINIM_NATIVE_SYS_EXIT, %eax
    int $0x80

1:
    hlt
    jmp 1b


.size _start, . - _start

.section .note.GNU-stack,"",@progbits
