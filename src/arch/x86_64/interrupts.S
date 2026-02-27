.intel_syntax noprefix

/**
 * @file interrupts.S
 * @brief Interrupt handlers for x86_64
 */

.section .text
.code64

.globl timer_interrupt_handler
.type timer_interrupt_handler, @function
.align 16
timer_interrupt_handler:
    # Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    # Save segment selectors
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax

    # Switch to kernel data segment
    mov ax, 0x10    
    mov ds, ax
    mov es, ax

    # Call C++ timer handler
    mov rdi, rsp
    call timer_interrupt_c_handler

    # Restore segment selectors
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    # Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq

.globl spurious_interrupt_handler
.type spurious_interrupt_handler, @function
.align 16
spurious_interrupt_handler:
    iretq

.globl generic_interrupt_handler
.type generic_interrupt_handler, @function
.align 16
generic_interrupt_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    call handle_unhandled_interrupt
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

.size timer_interrupt_handler, . - timer_interrupt_handler
.size spurious_interrupt_handler, . - spurious_interrupt_handler
.size generic_interrupt_handler, . - generic_interrupt_handler
