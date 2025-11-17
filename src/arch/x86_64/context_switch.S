/**
 * @file context_switch.S
 * @brief Low-level context switching for x86_64
 *
 * Provides assembly functions for saving/restoring CPU context
 * during process/thread switches.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

.section .text
.code64

/**
 * @brief Save current context and switch to next context
 * @param rdi Pointer to current process's CpuContext
 * @param rsi Pointer to next process's CpuContext
 *
 * C signature: void context_switch(CpuContext* current, CpuContext* next);
 *
 * This function saves the current CPU state into *current, then
 * restores the CPU state from *next and jumps to it.
 *
 * Layout of CpuContext (must match context.hpp):
 * +0x00: r15
 * +0x08: r14
 * +0x10: r13
 * +0x18: r12
 * +0x20: r11
 * +0x28: r10
 * +0x30: r9
 * +0x38: r8
 * +0x40: rbp
 * +0x48: rdi
 * +0x50: rsi
 * +0x58: rdx
 * +0x60: rcx
 * +0x68: rbx
 * +0x70: rax
 * +0x78: gs
 * +0x80: fs
 * +0x88: es
 * +0x90: ds
 * +0x98: rip
 * +0xA0: cs
 * +0xA8: rflags
 * +0xB0: rsp
 * +0xB8: ss
 * +0xC0: cr3
 */
.global context_switch
.type context_switch, @function
.align 16
context_switch:
    // Save current context
    // Note: rdi points to current, rsi points to next
    // We must save rdi and rsi too!

    // Save general purpose registers
    mov [rdi + 0x00], r15
    mov [rdi + 0x08], r14
    mov [rdi + 0x10], r13
    mov [rdi + 0x18], r12
    mov [rdi + 0x20], r11
    mov [rdi + 0x28], r10
    mov [rdi + 0x30], r9
    mov [rdi + 0x38], r8
    mov [rdi + 0x40], rbp

    // Save rdi (argument pointer, but we need original value)
    // Actually, we can just save current rdi since it's the context pointer
    mov [rdi + 0x48], rdi

    // Save rsi
    mov [rdi + 0x50], rsi

    mov [rdi + 0x58], rdx
    mov [rdi + 0x60], rcx
    mov [rdi + 0x68], rbx
    mov [rdi + 0x70], rax

    // Save segment selectors
    mov ax, gs
    mov [rdi + 0x78], rax
    mov ax, fs
    mov [rdi + 0x80], rax
    mov ax, es
    mov [rdi + 0x88], rax
    mov ax, ds
    mov [rdi + 0x90], rax

    // Save RIP (return address is on stack)
    mov rax, [rsp]
    mov [rdi + 0x98], rax

    // Save CS
    mov ax, cs
    mov [rdi + 0xA0], rax

    // Save RFLAGS
    pushfq
    pop rax
    mov [rdi + 0xA8], rax

    // Save RSP (add 8 because we'll pop the return address)
    lea rax, [rsp + 8]
    mov [rdi + 0xB0], rax

    // Save SS
    mov ax, ss
    mov [rdi + 0xB8], rax

    // Save CR3
    mov rax, cr3
    mov [rdi + 0xC0], rax

    // ========================================
    // Context saved! Now restore next context
    // ========================================

    // Load CR3 (switch page tables)
    mov rax, [rsi + 0xC0]
    cmp rax, 0
    je .skip_cr3  // Don't load CR3 if it's 0 (kernel page table)
    mov cr3, rax
.skip_cr3:

    // Restore segment selectors
    mov ax, [rsi + 0x78]
    mov gs, ax
    mov ax, [rsi + 0x80]
    mov fs, ax
    mov ax, [rsi + 0x88]
    mov es, ax
    mov ax, [rsi + 0x90]
    mov ds, ax

    // Restore RFLAGS
    mov rax, [rsi + 0xA8]
    push rax
    popfq

    // Restore stack pointer
    mov rsp, [rsi + 0xB0]

    // Restore general purpose registers
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
    // rsi will be restored last
    mov rdx, [rsi + 0x58]
    mov rcx, [rsi + 0x60]
    mov rbx, [rsi + 0x68]
    mov rax, [rsi + 0x70]

    // Push return address (RIP) onto new stack
    push qword ptr [rsi + 0x98]

    // Restore rsi last
    mov rsi, [rsi + 0x50]

    // Return (pops RIP from stack and jumps)
    ret


/**
 * @brief Load context and jump (for first-time context load)
 * @param rdi Pointer to CpuContext to load
 *
 * C signature: void load_context(CpuContext* ctx);
 *
 * This is used when there's no previous context to save.
 * It just loads the context and jumps to it.
 */
.global load_context
.type load_context, @function
.align 16
load_context:
    // Load CR3
    mov rax, [rdi + 0xC0]
    cmp rax, 0
    je .load_skip_cr3
    mov cr3, rax
.load_skip_cr3:

    // Restore segment selectors
    mov ax, [rdi + 0x78]
    mov gs, ax
    mov ax, [rdi + 0x80]
    mov fs, ax
    mov ax, [rdi + 0x88]
    mov es, ax
    mov ax, [rdi + 0x90]
    mov ds, ax

    // Restore RFLAGS
    mov rax, [rdi + 0xA8]
    push rax
    popfq

    // Restore stack pointer
    mov rsp, [rdi + 0xB0]

    // Restore general purpose registers
    mov r15, [rdi + 0x00]
    mov r14, [rdi + 0x08]
    mov r13, [rdi + 0x10]
    mov r12, [rdi + 0x18]
    mov r11, [rdi + 0x20]
    mov r10, [rdi + 0x28]
    mov r9,  [rdi + 0x30]
    mov r8,  [rdi + 0x38]
    mov rbp, [rdi + 0x40]
    // Skip rdi for now
    mov rsi, [rdi + 0x50]
    mov rdx, [rdi + 0x58]
    mov rcx, [rdi + 0x60]
    mov rbx, [rdi + 0x68]
    mov rax, [rdi + 0x70]

    // Push return address
    push qword ptr [rdi + 0x98]

    // Restore rdi last
    mov rdi, [rdi + 0x48]

    // Jump to RIP
    ret


/**
 * @brief Load context and transition to Ring 3
 * @param rdi Pointer to CpuContext to load
 *
 * C signature: [[noreturn]] void load_context_ring3(CpuContext* ctx);
 *
 * This is used to transition from Ring 0 (kernel) to Ring 3 (user).
 * It uses iretq which pops SS, RSP, RFLAGS, CS, RIP from stack.
 */
.global load_context_ring3
.type load_context_ring3, @function
.align 16
load_context_ring3:
    // Load CR3
    mov rax, [rdi + 0xC0]
    cmp rax, 0
    je .ring3_skip_cr3
    mov cr3, rax
.ring3_skip_cr3:

    // Build interrupt frame on stack for iretq
    // iretq expects: SS, RSP, RFLAGS, CS, RIP (in that order, bottom to top)
    push qword ptr [rdi + 0xB8]  // SS
    push qword ptr [rdi + 0xB0]  // RSP
    push qword ptr [rdi + 0xA8]  // RFLAGS
    push qword ptr [rdi + 0xA0]  // CS
    push qword ptr [rdi + 0x98]  // RIP

    // Restore segment selectors (data segments only, CS handled by iretq)
    mov ax, [rdi + 0x90]
    mov ds, ax
    mov ax, [rdi + 0x88]
    mov es, ax
    mov ax, [rdi + 0x80]
    mov fs, ax
    mov ax, [rdi + 0x78]
    mov gs, ax

    // Restore general purpose registers
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

    // Restore rdi last
    mov rdi, [rdi + 0x48]

    // Return to Ring 3 (pops RIP, CS, RFLAGS, RSP, SS)
    iretq


.size context_switch, . - context_switch
.size load_context, . - load_context
.size load_context_ring3, . - load_context_ring3
