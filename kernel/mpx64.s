.globl isr_default, isr_clock, isr_keyboard
.globl _s_call, _tty_int, _lpr_int, _disk_int, _surprise, _trp, _divide
.globl _restart
.globl save

.set RAX_OFF, 0
.set RBX_OFF, 8
.set RCX_OFF, 16
.set RDX_OFF, 24
.set RSI_OFF, 32
.set RDI_OFF, 40
.set RBP_OFF, 48
.set R8_OFF, 56
.set R9_OFF, 64
.set R10_OFF, 72
.set R11_OFF, 80
.set R12_OFF, 88
.set R13_OFF, 96
.set R14_OFF, 104
.set R15_OFF, 112
.set SP_OFF, 120
.set PC_OFF, 128
.set PSW_OFF,136

.text

/* Save registers to proc table and switch to kernel stack. */
save:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    movq _proc_ptr(%rip), %r15
    movq 120(%rsp), %rax
    movq %rax, PC_OFF(%r15)
    movq 136(%rsp), %rax
    movq %rax, PSW_OFF(%r15)
    lea 120(%rsp), %rax
    movq %rax, SP_OFF(%r15)
    movq 112(%rsp), %rax
    movq %rax, RAX_OFF(%r15)
    movq 104(%rsp), %rax
    movq %rax, RBX_OFF(%r15)
    movq 96(%rsp), %rax
    movq %rax, RCX_OFF(%r15)
    movq 88(%rsp), %rax
    movq %rax, RDX_OFF(%r15)
    movq 80(%rsp), %rax
    movq %rax, RSI_OFF(%r15)
    movq 72(%rsp), %rax
    movq %rax, RDI_OFF(%r15)
    movq 64(%rsp), %rax
    movq %rax, RBP_OFF(%r15)
    movq 56(%rsp), %rax
    movq %rax, R8_OFF(%r15)
    movq 48(%rsp), %rax
    movq %rax, R9_OFF(%r15)
    movq 40(%rsp), %rax
    movq %rax, R10_OFF(%r15)
    movq 32(%rsp), %rax
    movq %rax, R11_OFF(%r15)
    movq 24(%rsp), %rax
    movq %rax, R12_OFF(%r15)
    movq 16(%rsp), %rax
    movq %rax, R13_OFF(%r15)
    movq 8(%rsp), %rax
    movq %rax, R14_OFF(%r15)
    movq (%rsp), %rax
    movq %rax, R15_OFF(%r15)
    lea _k_stack+K_STACK_BYTES(%rip), %rsp
    ret

/* Restore registers and return from interrupt. */
_restart:
    movq _proc_ptr(%rip), %r15
    movq SP_OFF(%r15), %rsp
    movq R15_OFF(%r15), %r15
    movq R14_OFF(%r15), %r14
    movq R13_OFF(%r15), %r13
    movq R12_OFF(%r15), %r12
    movq R11_OFF(%r15), %r11
    movq R10_OFF(%r15), %r10
    movq R9_OFF(%r15), %r9
    movq R8_OFF(%r15), %r8
    movq RBP_OFF(%r15), %rbp
    movq RDI_OFF(%r15), %rdi
    movq RSI_OFF(%r15), %rsi
    movq RDX_OFF(%r15), %rdx
    movq RCX_OFF(%r15), %rcx
    movq RBX_OFF(%r15), %rbx
    movq RAX_OFF(%r15), %rax
    pushq PSW_OFF(%r15)
    pushq PC_OFF(%r15)
    iretq

/* Default ISR just calls surprise and returns */
isr_default:
    call save
    call _surprise
    jmp _restart

isr_clock:
    call save
    call _clock_int
    jmp _restart

isr_keyboard:
    call save
    call _tty_int
    jmp _restart

_s_call:
    call save
    movq _proc_ptr(%rip), %rdi
    movq 16(%rdi), %rsi
    movq (%rdi), %rdx
    movq $0, %rcx
    call _sys_call
    jmp _restart

_lpr_int:
    call save
    call _pr_char
    jmp _restart

_disk_int:
    call save
    movq _int_mess+2(%rip), %rax
    movq %rax, %rdi
    call _interrupt
    jmp _restart

_divide:
    call save
    call _div_trap
    jmp _restart

_trp:
    call save
    call _trap
    jmp _restart

