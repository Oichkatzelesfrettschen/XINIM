.intel_syntax noprefix
.globl isr_stub_32
.type isr_stub_32, @function
isr_stub_32:
    push rbp
    mov rbp, rsp
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    mov rdi, 32               # pass vector number
    call isr_common_handler
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    leave
    iretq

.size isr_stub_32, .-isr_stub_32

.globl lidt
.type lidt, @function
lidt:
    mov rax, rdi
    lidt [rax]
    ret
.size lidt, .-lidt

