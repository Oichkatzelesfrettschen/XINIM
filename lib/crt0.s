# 64-bit startup code for cross-compiled programs
# Provide _start entry, set up arguments and call _main.

.globl _start, _main, _exit, _environ
.text
_start:
    movq %rsp, %rdi          # rdi -> argc pointer
    mov (%rdi), %edi         # edi = argc
    lea 8(%rsp), %rsi        # rsi -> argv
    lea 1(,%rdi,1), %rdx     # rdx = argc + 1
    shl $3, %rdx             # convert to byte offset
    add %rsi, %rdx           # rdx -> envp
    mov %rdx, _environ(%rip) # store environ
    call _main               # jump to main
    mov %eax, %edi           # pass return code
    call _exit               # exit process

.data
.globl _environ
_environ:
    .quad 0

