.globl _start, _main, _exit, _environ, _stackpt
.text
_start:
    movq %rsp, %rdi          # pointer to argc
    mov (%rdi), %edi         # argc
    lea 8(%rsp), %rsi        # argv
    lea 1(,%rdi,1), %rdx     # argc + 1
    shl $3, %rdx
    add %rsi, %rdx           # envp
    mov %rdx, _environ(%rip)
    call _main
    mov %eax, %edi
    call _exit

.data
.globl _environ
_environ:
    .quad 0
