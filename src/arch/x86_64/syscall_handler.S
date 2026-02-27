.intel_syntax noprefix

/**
 * @file syscall_handler.S
 * @brief Fast syscall entry point for x86_64
 */

.section .text
.code64

.globl syscall_handler
.type syscall_handler, @function
.align 16
syscall_handler:
    # CRITICAL: We are in Ring 0 but still on user stack!
    # Save user RSP in R15 temporarily
    mov r15, rsp

    # Switch to kernel syscall stack
    lea rsp, [rip + kernel_syscall_stack_top]

    # Now we're on kernel stack - safe to save everything
    push r15        # User RSP
    push r11        # User RFLAGS
    push rcx        # User RIP

    # Save callee-saved registers
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15        # Save original R15

    # Save argument registers
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    # Save RAX (syscall number)
    push rax

    # Prepare arguments for syscall_dispatch
    # User passed: RAX=syscall_num, RDI=arg1, RSI=arg2, RDX=arg3, R10=arg4, R8=arg5, R9=arg6
    # C++ expects: RDI=syscall_num, RSI=arg1, RDX=arg2, RCX=arg3, R8=arg4, R9=arg5, stack=arg6

    mov rdi, [rsp]         # RAX (syscall_num) -> RDI
    mov rsi, [rsp + 8*6]   # Original RDI (arg1) -> RSI
    mov rdx, [rsp + 8*5]   # Original RSI (arg2) -> RDX
    mov rcx, [rsp + 8*4]   # Original RDX (arg3) -> RCX
    mov r8,  [rsp + 8*3]   # Original R10 (arg4) -> R8
    mov r9,  [rsp + 8*2]   # Original R8 (arg5) -> R9

    # arg6 is original R9
    mov rax, [rsp + 8*1]   
    push rax

    # Call C++ dispatch function (renamed to match dispatch.cpp or syscall.cpp)
    # The linker error said 'xinim_syscall_dispatch' was defined in dispatch.cpp
    call xinim_syscall_dispatch

    # Clean up arg6 from stack
    add rsp, 8

    # RAX now contains return value - save in callee-saved register
    mov rbx, rax

    # Restore registers (skip argument registers)
    add rsp, 8*7    

    # Restore callee-saved registers (except RBX)
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    add rsp, 8      

    pop rcx         # User RIP
    pop r11         # User RFLAGS
    pop rsp         # User RSP

    mov rax, rbx
    sysretq

.size syscall_handler, . - syscall_handler

.section .bss
.align 16
kernel_syscall_stack:
    .skip 8192
kernel_syscall_stack_top:
