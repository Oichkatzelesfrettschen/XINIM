/**
 * @file syscall_handler.S
 * @brief Fast syscall entry point for x86_64
 *
 * Handles Ring 3 → Ring 0 transition via syscall instruction.
 * Saves user context, switches to kernel stack, dispatches to C++ handler,
 * and returns to Ring 3 via sysret.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

.section .text
.code64

/**
 * @brief Syscall entry point
 *
 * Entry conditions:
 * - RCX = user RIP (saved by CPU)
 * - R11 = user RFLAGS (saved by CPU)
 * - RAX = syscall number
 * - RDI = arg1
 * - RSI = arg2
 * - RDX = arg3
 * - R10 = arg4 (syscall uses R10 instead of RCX)
 * - R8  = arg5
 * - R9  = arg6
 * - RSP = user stack pointer (NOT switched by CPU!)
 * - CS = kernel CS (switched by CPU)
 * - SS = user SS (NOT switched by CPU!)
 *
 * We must:
 * 1. Switch to kernel stack manually
 * 2. Save all registers
 * 3. Call dispatch function
 * 4. Restore registers
 * 5. Switch back to user stack
 * 6. Return via sysret
 */
.global syscall_handler
.type syscall_handler, @function
.align 16
syscall_handler:
    // CRITICAL: We are in Ring 0 but still on user stack!
    // We must switch to kernel stack immediately

    // Save user RSP in a scratch register
    // We'll use R15 temporarily (will save it later)
    mov r15, rsp

    // Load kernel RSP from TSS
    // The TSS.rsp0 was set by schedule() before switching processes
    // For Week 8, we'll use a simpler approach: per-process kernel stack

    // TODO: For now, we'll use a temporary kernel stack
    // In production, read from per-CPU data or TSS

    // Simplified for Week 8: Use static kernel syscall stack
    // This works because syscalls are atomic (we disabled interrupts via FMASK)
    lea rsp, [rip + kernel_syscall_stack_top]

    // Now we're on kernel stack - safe to save everything

    // Save user RSP (from R15)
    push r15        // User RSP

    // Save registers that sysret will restore
    push r11        // User RFLAGS
    push rcx        // User RIP

    // Save callee-saved registers (required by ABI)
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15        // Save R15 again (we used it for RSP transfer)

    // Save argument registers (might be clobbered)
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    // Save RAX (syscall number)
    push rax

    // At this point, stack layout (from top to bottom):
    // [RAX] [R9] [R8] [R10] [RDX] [RSI] [RDI] [R15] [R14] [R13] [R12] [RBP] [RBX] [RCX] [R11] [user RSP]
    //  0     1     2     3      4      5      6      7      8      9     10     11    12    13     14     15

    // Prepare arguments for syscall_dispatch
    // C signature: int64_t syscall_dispatch(syscall_num, arg1, arg2, arg3, arg4, arg5, arg6)
    //
    // User passed:
    // RAX = syscall_num, RDI = arg1, RSI = arg2, RDX = arg3, R10 = arg4, R8 = arg5, R9 = arg6
    //
    // C++ expects:
    // RDI = syscall_num, RSI = arg1, RDX = arg2, RCX = arg3, R8 = arg4, R9 = arg5, stack = arg6
    //
    // Rearrange from stack (safer than complex register shuffling):

    mov rdi, [rsp]         // RAX (syscall_num) → RDI
    mov rsi, [rsp + 8*6]   // Original RDI (arg1) → RSI
    mov rdx, [rsp + 8*5]   // Original RSI (arg2) → RDX
    mov rcx, [rsp + 8*4]   // Original RDX (arg3) → RCX
    mov r8,  [rsp + 8*3]   // Original R10 (arg4) → R8
    mov r9,  [rsp + 8*2]   // Original R8 (arg5) → R9

    // arg6 is original R9, push it on stack for 7th argument
    mov rax, [rsp + 8*1]   // Original R9 (arg6)
    push rax

    // Call C++ dispatch function
    call syscall_dispatch

    // Clean up arg6 from stack
    add rsp, 8

    // RAX now contains return value - save in callee-saved register
    mov rbx, rax

    // Restore registers (skip argument registers - we don't care about their values)
    add rsp, 8*7    // Skip RAX, R9, R8, R10, RDX, RSI, RDI

    // Restore callee-saved registers (except RBX - it has our return value)
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    add rsp, 8      // Skip RBX (we're using it for return value)

    pop rcx         // User RIP
    pop r11         // User RFLAGS
    pop rsp         // User RSP - switch back to user stack!

    // Move return value to RAX
    mov rax, rbx

    // Return to user mode
    // sysretq uses: RCX = user RIP, R11 = user RFLAGS
    sysretq

.size syscall_handler, . - syscall_handler

// ============================================================================
// Kernel Syscall Stack (temporary for Week 8)
// ============================================================================

.section .bss
.align 16
kernel_syscall_stack:
    .skip 8192  // 8 KB kernel stack for syscalls
kernel_syscall_stack_top:
