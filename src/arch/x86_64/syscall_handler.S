#include <xinim/abi/x86_64_context_layout.h>
#include <xinim/abi/x86_64_syscall_frame.h>

.intel_syntax noprefix

/**
 * @file syscall_handler.S
 * @brief SysV AMD64 bridge from SYSCALL to the XINIM C++ dispatcher
 */

.section .text
.code64

.extern xinim_syscall_dispatch_frame

.globl syscall_handler
.type syscall_handler, @function
.p2align 4
syscall_handler:
    /*
     * SYSCALL does not switch stacks. The kernel is single-CPU, so the entry
     * path first records the user stack in dedicated scratch storage and then
     * moves to the kernel syscall stack without consuming a user GPR.
     */
    mov [rip + syscall_user_rsp], rsp
    mov rsp, [rip + syscall_kernel_stack_top]

    /* Explicit IRETQ frame; the current GDT is not a valid SYSRET pair. */
    push XINIM_X86_USER_DS_SELECTOR
    push qword ptr [rip + syscall_user_rsp]
    push r11
    push XINIM_X86_USER_CS_SELECTOR
    push rcx

    /* Preserve every user GPR except architectural RCX and R11 clobbers. */
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9
    push rax

    /* C and C++ require a clear direction flag and a 16-byte-aligned call. */
    cld

    /* The dispatcher may use x87, MMX, or SSE registers. Preserve user state. */
    sub rsp, XINIM_X86_64_FXSAVE_SIZE
    fxsave64 [rsp]

    lea rdi, [rsp + XINIM_X86_64_FXSAVE_SIZE]
    call xinim_syscall_dispatch_frame

    fxrstor64 [rsp]
    add rsp, XINIM_X86_64_FXSAVE_SIZE

    /* Replace the saved syscall number with the dispatcher result. */
    mov [rsp + XINIM_X86_64_SYSCALL_RAX], rax

    pop rax
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    iretq

.size syscall_handler, . - syscall_handler

.section .bss
.p2align 4
syscall_user_rsp:
    .quad 0

.p2align 4
kernel_syscall_stack:
    .skip 8192
kernel_syscall_stack_top:

.section .data
.globl syscall_kernel_stack_top
.type syscall_kernel_stack_top, @object
.p2align 3
syscall_kernel_stack_top:
    .quad kernel_syscall_stack_top

.section .note.GNU-stack,"",@progbits
