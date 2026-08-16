#include <xinim/abi/x86_64_context_layout.h>

.intel_syntax noprefix

/**
 * @file context_restore.S
 * @brief Non-returning x86_64 context restore entry points
 */

.section .text
.code64

.globl load_context
.type load_context, @function
.align 16
load_context:
    cli

    mov rax, [rdi + XINIM_X86_64_CONTEXT_CR3]
    test rax, rax
    jz .load_skip_cr3
    mov cr3, rax
.load_skip_cr3:

    fxrstor64 [rdi + XINIM_X86_64_CONTEXT_FXSAVE]

    mov ax, [rdi + XINIM_X86_64_CONTEXT_GS]
    mov gs, ax
    mov ax, [rdi + XINIM_X86_64_CONTEXT_FS]
    mov fs, ax
    mov ax, [rdi + XINIM_X86_64_CONTEXT_ES]
    mov es, ax
    mov ax, [rdi + XINIM_X86_64_CONTEXT_DS]
    mov ds, ax

    # Use the destination stack for a same-privilege IRETQ frame. RFLAGS,
    # including IF, takes effect only after the complete context is restored.
    mov rsp, [rdi + XINIM_X86_64_CONTEXT_RSP]
    push [rdi + XINIM_X86_64_CONTEXT_RFLAGS]
    push [rdi + XINIM_X86_64_CONTEXT_CS]
    push [rdi + XINIM_X86_64_CONTEXT_RIP]

    mov r15, [rdi + XINIM_X86_64_CONTEXT_R15]
    mov r14, [rdi + XINIM_X86_64_CONTEXT_R14]
    mov r13, [rdi + XINIM_X86_64_CONTEXT_R13]
    mov r12, [rdi + XINIM_X86_64_CONTEXT_R12]
    mov r11, [rdi + XINIM_X86_64_CONTEXT_R11]
    mov r10, [rdi + XINIM_X86_64_CONTEXT_R10]
    mov r9,  [rdi + XINIM_X86_64_CONTEXT_R9]
    mov r8,  [rdi + XINIM_X86_64_CONTEXT_R8]
    mov rbp, [rdi + XINIM_X86_64_CONTEXT_RBP]
    mov rsi, [rdi + XINIM_X86_64_CONTEXT_RSI]
    mov rdx, [rdi + XINIM_X86_64_CONTEXT_RDX]
    mov rcx, [rdi + XINIM_X86_64_CONTEXT_RCX]
    mov rbx, [rdi + XINIM_X86_64_CONTEXT_RBX]
    mov rax, [rdi + XINIM_X86_64_CONTEXT_RAX]

    mov rdi, [rdi + XINIM_X86_64_CONTEXT_RDI]
    iretq

.size load_context, . - load_context

.globl load_context_ring3
.type load_context_ring3, @function
.align 16
load_context_ring3:
    cli

    mov rax, [rdi + XINIM_X86_64_CONTEXT_CR3]
    test rax, rax
    jz .ring3_skip_cr3
    mov cr3, rax
.ring3_skip_cr3:

    fxrstor64 [rdi + XINIM_X86_64_CONTEXT_FXSAVE]

    # Build interrupt frame
    push [rdi + XINIM_X86_64_CONTEXT_SS]
    push [rdi + XINIM_X86_64_CONTEXT_RSP]
    push [rdi + XINIM_X86_64_CONTEXT_RFLAGS]
    push [rdi + XINIM_X86_64_CONTEXT_CS]
    push [rdi + XINIM_X86_64_CONTEXT_RIP]

    mov ax, [rdi + XINIM_X86_64_CONTEXT_DS]
    mov ds, ax
    mov ax, [rdi + XINIM_X86_64_CONTEXT_ES]
    mov es, ax
    mov ax, [rdi + XINIM_X86_64_CONTEXT_FS]
    mov fs, ax
    mov ax, [rdi + XINIM_X86_64_CONTEXT_GS]
    mov gs, ax

    mov r15, [rdi + XINIM_X86_64_CONTEXT_R15]
    mov r14, [rdi + XINIM_X86_64_CONTEXT_R14]
    mov r13, [rdi + XINIM_X86_64_CONTEXT_R13]
    mov r12, [rdi + XINIM_X86_64_CONTEXT_R12]
    mov r11, [rdi + XINIM_X86_64_CONTEXT_R11]
    mov r10, [rdi + XINIM_X86_64_CONTEXT_R10]
    mov r9,  [rdi + XINIM_X86_64_CONTEXT_R9]
    mov r8,  [rdi + XINIM_X86_64_CONTEXT_R8]
    mov rbp, [rdi + XINIM_X86_64_CONTEXT_RBP]
    mov rsi, [rdi + XINIM_X86_64_CONTEXT_RSI]
    mov rdx, [rdi + XINIM_X86_64_CONTEXT_RDX]
    mov rcx, [rdi + XINIM_X86_64_CONTEXT_RCX]
    mov rbx, [rdi + XINIM_X86_64_CONTEXT_RBX]
    mov rax, [rdi + XINIM_X86_64_CONTEXT_RAX]
    mov rdi, [rdi + XINIM_X86_64_CONTEXT_RDI]
    iretq

.size load_context_ring3, . - load_context_ring3

.section .note.GNU-stack,"",@progbits
