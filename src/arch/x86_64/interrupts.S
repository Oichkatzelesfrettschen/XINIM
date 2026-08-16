#include <xinim/abi/x86_64_context_layout.h>

.intel_syntax noprefix

/**
 * @file interrupts.S
 * @brief x86_64 interrupt gates and SysV C++ call bridge
 */

.section .text
.code64

.macro XINIM_PUSH_INTERRUPT_STATE
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

    xor eax, eax
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax
.endm

.macro XINIM_CALL_INTERRUPT_HANDLER target
    mov ax, XINIM_X86_KERNEL_DS_SELECTOR
    mov ds, ax
    mov es, ax

    /* Preserve the frame base while aligning an arbitrary interrupted RSP. */
    mov rbp, rsp
    mov rdi, rsp
    and rsp, -16
    sub rsp, XINIM_X86_64_FXSAVE_SIZE
    fxsave64 [rsp]
    mov rsi, rsp
    cld
    call \target
    fxrstor64 [rsp]
    mov rsp, rbp
.endm

.macro XINIM_POP_INTERRUPT_STATE
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

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
.endm

.macro XINIM_INTERRUPT_GATE name, callback
    .globl \name
    .type \name, @function
    .p2align 4
\name:
    XINIM_PUSH_INTERRUPT_STATE
    XINIM_CALL_INTERRUPT_HANDLER \callback
    XINIM_POP_INTERRUPT_STATE
    iretq
    .size \name, . - \name
.endm

XINIM_INTERRUPT_GATE timer_interrupt_handler, timer_interrupt_c_handler
XINIM_INTERRUPT_GATE generic_interrupt_handler, handle_unhandled_irq
XINIM_INTERRUPT_GATE keyboard_interrupt_handler, keyboard_interrupt_c_handler
XINIM_INTERRUPT_GATE com1_interrupt_handler, com1_interrupt_c_handler
XINIM_INTERRUPT_GATE com2_interrupt_handler, com2_interrupt_c_handler

.macro XINIM_EXCEPTION_GATE_NO_ERROR vector
    .globl x86_64_exception_\vector
    .type x86_64_exception_\vector, @function
    .p2align 4
x86_64_exception_\vector:
    push 0
    push \vector
    jmp x86_64_exception_common
    .size x86_64_exception_\vector, . - x86_64_exception_\vector
.endm

.macro XINIM_EXCEPTION_GATE_ERROR vector
    .globl x86_64_exception_\vector
    .type x86_64_exception_\vector, @function
    .p2align 4
x86_64_exception_\vector:
    push \vector
    jmp x86_64_exception_common
    .size x86_64_exception_\vector, . - x86_64_exception_\vector
.endm

XINIM_EXCEPTION_GATE_NO_ERROR 0
XINIM_EXCEPTION_GATE_NO_ERROR 1
XINIM_EXCEPTION_GATE_NO_ERROR 2
XINIM_EXCEPTION_GATE_NO_ERROR 3
XINIM_EXCEPTION_GATE_NO_ERROR 4
XINIM_EXCEPTION_GATE_NO_ERROR 5
XINIM_EXCEPTION_GATE_NO_ERROR 6
XINIM_EXCEPTION_GATE_NO_ERROR 7
XINIM_EXCEPTION_GATE_ERROR 8
XINIM_EXCEPTION_GATE_NO_ERROR 9
XINIM_EXCEPTION_GATE_ERROR 10
XINIM_EXCEPTION_GATE_ERROR 11
XINIM_EXCEPTION_GATE_ERROR 12
XINIM_EXCEPTION_GATE_ERROR 13
XINIM_EXCEPTION_GATE_ERROR 14
XINIM_EXCEPTION_GATE_NO_ERROR 15
XINIM_EXCEPTION_GATE_NO_ERROR 16
XINIM_EXCEPTION_GATE_ERROR 17
XINIM_EXCEPTION_GATE_NO_ERROR 18
XINIM_EXCEPTION_GATE_NO_ERROR 19
XINIM_EXCEPTION_GATE_NO_ERROR 20
XINIM_EXCEPTION_GATE_ERROR 21
XINIM_EXCEPTION_GATE_NO_ERROR 22
XINIM_EXCEPTION_GATE_NO_ERROR 23
XINIM_EXCEPTION_GATE_NO_ERROR 24
XINIM_EXCEPTION_GATE_NO_ERROR 25
XINIM_EXCEPTION_GATE_NO_ERROR 26
XINIM_EXCEPTION_GATE_NO_ERROR 27
XINIM_EXCEPTION_GATE_NO_ERROR 28
XINIM_EXCEPTION_GATE_ERROR 29
XINIM_EXCEPTION_GATE_ERROR 30
XINIM_EXCEPTION_GATE_NO_ERROR 31

.globl x86_64_exception_common
.type x86_64_exception_common, @function
.p2align 4
x86_64_exception_common:
    XINIM_PUSH_INTERRUPT_STATE
    XINIM_CALL_INTERRUPT_HANDLER handle_unhandled_exception
    XINIM_POP_INTERRUPT_STATE
    add rsp, 16
    iretq
.size x86_64_exception_common, . - x86_64_exception_common

.globl spurious_interrupt_handler
.type spurious_interrupt_handler, @function
.p2align 4
spurious_interrupt_handler:
    /* A local-APIC spurious interrupt requires no EOI. */
    iretq
.size spurious_interrupt_handler, . - spurious_interrupt_handler

.globl lidt
.type lidt, @function
lidt:
    lidt [rdi]
    ret
.size lidt, . - lidt

.section .note.GNU-stack,"",@progbits
