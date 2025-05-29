.globl _setjmp, _longjmp
.text
_setjmp:
    movq %rbx, (%rdi)
    movq %rbp, 8(%rdi)
    movq %rsp, 16(%rdi)
    movq (%rsp), %rax
    movq %rax, 24(%rdi)
    xor %eax, %eax
    ret

_longjmp:
    movq (%rdi), %rbx
    movq 8(%rdi), %rbp
    movq 16(%rdi), %rsp
    movq 24(%rdi), %rcx
    test %esi, %esi
    jne 1f
    movl $1, %esi
1:  mov %esi, %eax
    jmp *%rcx
