.intel_syntax noprefix

/**
 * @file context_switch.S
 * @brief Low-level context switching for x86_64
 */

.section .text
.code64

.globl context_switch
.type context_switch, @function
.align 16
context_switch:
    # Save general purpose registers
    mov [rdi + 0x00], r15
    mov [rdi + 0x08], r14
    mov [rdi + 0x10], r13
    mov [rdi + 0x18], r12
    mov [rdi + 0x20], r11
    mov [rdi + 0x28], r10
    mov [rdi + 0x30], r9
    mov [rdi + 0x38], r8
    mov [rdi + 0x40], rbp
    mov [rdi + 0x48], rdi
    mov [rdi + 0x50], rsi
    mov [rdi + 0x58], rdx
    mov [rdi + 0x60], rcx
    mov [rdi + 0x68], rbx
    mov [rdi + 0x70], rax

    # Save segment selectors
    mov ax, gs
    mov [rdi + 0x78], rax
    mov ax, fs
    mov [rdi + 0x80], rax
    mov ax, es
    mov [rdi + 0x88], rax
    mov ax, ds
    mov [rdi + 0x90], rax

    # Save RIP
    mov rax, [rsp]
    mov [rdi + 0x98], rax

    # Save CS
    mov ax, cs
    mov [rdi + 0xA0], rax

    # Save RFLAGS
    pushfq
    pop rax
    mov [rdi + 0xA8], rax

    # Save RSP
    lea rax, [rsp + 8]
    mov [rdi + 0xB0], rax

    # Save SS
    mov ax, ss
    mov [rdi + 0xB8], rax

    # Save CR3
    mov rax, cr3
    mov [rdi + 0xC0], rax

    # Restore next context
    mov rax, [rsi + 0xC0]
    test rax, rax
    jz .skip_cr3
    mov cr3, rax
.skip_cr3:

    mov ax, [rsi + 0x78]
    mov gs, ax
    mov ax, [rsi + 0x80]
    mov fs, ax
    mov ax, [rsi + 0x88]
    mov es, ax
    mov ax, [rsi + 0x90]
    mov ds, ax

    mov rax, [rsi + 0xA8]
    push rax
    popfq

    mov rsp, [rsi + 0xB0]

    mov r15, [rsi + 0x00]
    mov r14, [rsi + 0x08]
    mov r13, [rsi + 0x10]
    mov r12, [rsi + 0x18]
    mov r11, [rsi + 0x20]
    mov r10, [rsi + 0x28]
    mov r9,  [rsi + 0x30]
    mov r8,  [rsi + 0x38]
    mov rbp, [rsi + 0x40]
    mov rdi, [rsi + 0x48]
    mov rdx, [rsi + 0x58]
    mov rcx, [rsi + 0x60]
    mov rbx, [rsi + 0x68]
    mov rax, [rsi + 0x70]

    # Push return address
    push [rsi + 0x98]

    mov rsi, [rsi + 0x50]
    ret

.globl load_context
.type load_context, @function
.align 16
load_context:
    mov rax, [rdi + 0xC0]
    test rax, rax
    jz .load_skip_cr3
    mov cr3, rax
.load_skip_cr3:

    mov ax, [rdi + 0x78]
    mov gs, ax
    mov ax, [rdi + 0x80]
    mov fs, ax
    mov ax, [rdi + 0x88]
    mov es, ax
    mov ax, [rdi + 0x90]
    mov ds, ax

    mov rax, [rdi + 0xA8]
    push rax
    popfq

    mov rsp, [rdi + 0xB0]

    mov r15, [rdi + 0x00]
    mov r14, [rdi + 0x08]
    mov r13, [rdi + 0x10]
    mov r12, [rdi + 0x18]
    mov r11, [rdi + 0x20]
    mov r10, [rdi + 0x28]
    mov r9,  [rdi + 0x30]
    mov r8,  [rdi + 0x38]
    mov rbp, [rdi + 0x40]
    mov rsi, [rdi + 0x50]
    mov rdx, [rdi + 0x58]
    mov rcx, [rdi + 0x60]
    mov rbx, [rdi + 0x68]
    mov rax, [rdi + 0x70]

    push [rdi + 0x98]
    mov rdi, [rdi + 0x48]
    ret

.globl load_context_ring3
.type load_context_ring3, @function
.align 16
load_context_ring3:
    mov rax, [rdi + 0xC0]
    test rax, rax
    jz .ring3_skip_cr3
    mov cr3, rax
.ring3_skip_cr3:

    # Build interrupt frame
    push [rdi + 0xB8]  # SS
    push [rdi + 0xB0]  # RSP
    push [rdi + 0xA8]  # RFLAGS
    push [rdi + 0xA0]  # CS
    push [rdi + 0x98]  # RIP

    mov ax, [rdi + 0x90]
    mov ds, ax
    mov ax, [rdi + 0x88]
    mov es, ax
    mov ax, [rdi + 0x80]
    mov fs, ax
    mov ax, [rdi + 0x78]
    mov gs, ax

    mov r15, [rdi + 0x00]
    mov r14, [rdi + 0x08]
    mov r13, [rdi + 0x10]
    mov r12, [rdi + 0x18]
    mov r11, [rdi + 0x20]
    mov r10, [rdi + 0x28]
    mov r9,  [rdi + 0x30]
    mov r8,  [rdi + 0x38]
    mov rbp, [rdi + 0x40]
    mov rsi, [rdi + 0x50]
    mov rdx, [rdi + 0x58]
    mov rcx, [rdi + 0x60]
    mov rbx, [rdi + 0x68]
    mov rax, [rdi + 0x70]
    mov rdi, [rdi + 0x48]
    iretq

.size context_switch, . - context_switch
.size load_context, . - load_context
.size load_context_ring3, . - load_context_ring3
