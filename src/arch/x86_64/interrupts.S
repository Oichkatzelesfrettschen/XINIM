/**
 * @file interrupts.S
 * @brief Interrupt handlers for x86_64
 *
 * Provides low-level interrupt handlers, including the critical
 * timer interrupt for preemptive multitasking.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

.section .text
.code64

/**
 * @brief Timer interrupt handler (IRQ 0, Vector 32)
 *
 * This handler is invoked ~100 times per second by the APIC timer.
 * It saves the current process context, calls the scheduler to pick
 * the next process, and restores the new process context.
 *
 * Entry State (set by CPU on interrupt):
 * - Stack has: SS, RSP, RFLAGS, CS, RIP (pushed by hardware)
 * - Interrupts are disabled (IF=0)
 * - CPL changed to 0 (kernel mode)
 *
 * Exit State:
 * - Next process's context fully restored
 * - Interrupts re-enabled (via RFLAGS from iretq)
 * - Execution continues in next process
 */
.global timer_interrupt_handler
.type timer_interrupt_handler, @function
.align 16
timer_interrupt_handler:
    // At this point, CPU has pushed: SS, RSP, RFLAGS, CS, RIP
    // We need to save the rest of the context

    // Save all general purpose registers (in reverse order for context structure)
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

    // Save segment selectors
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax

    // At this point, RSP points to a complete CpuContext on the stack
    // Stack layout (top to bottom):
    // +0x00: gs
    // +0x08: fs
    // +0x10: es
    // +0x18: ds
    // +0x20: r15
    // +0x28: r14
    // ...
    // +0x78: rax
    // +0x80: RIP (pushed by CPU)
    // +0x88: CS (pushed by CPU)
    // +0x90: RFLAGS (pushed by CPU)
    // +0x98: RSP (pushed by CPU)
    // +0xA0: SS (pushed by CPU)

    // Switch to kernel data segment
    mov ax, 0x10    // Kernel data segment selector
    mov ds, ax
    mov es, ax

    // Call C++ timer handler
    // Pass pointer to saved context as argument
    mov rdi, rsp
    call timer_interrupt_c_handler

    // The C++ handler may have switched contexts!
    // RSP might now point to a different process's context.

    // Restore segment selectors
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    // Restore general purpose registers
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

    // Send EOI (End of Interrupt) to APIC
    // This must be done before iretq to allow next interrupt
    // APIC EOI register is at 0xFEE000B0 (physical)
    // With HHDM offset, it's at a virtual address
    // For now, we'll do this in C code before returning

    // Return from interrupt
    // This pops: RIP, CS, RFLAGS, RSP, SS
    // and jumps to the restored RIP
    iretq


/**
 * @brief Spurious interrupt handler
 *
 * APIC can generate spurious interrupts (vector 255).
 * We just acknowledge and return.
 */
.global spurious_interrupt_handler
.type spurious_interrupt_handler, @function
.align 16
spurious_interrupt_handler:
    // No EOI needed for spurious interrupts
    iretq


/**
 * @brief Generic interrupt handler (for debugging)
 *
 * Used for unhandled interrupts. Prints vector number and halts.
 */
.global generic_interrupt_handler
.type generic_interrupt_handler, @function
.align 16
generic_interrupt_handler:
    // Save context
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    // Call C++ handler (will print error and halt)
    call handle_unhandled_interrupt

    // Restore and return (won't reach here if handler halts)
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
