#include <xinim/abi/x86_32_context_layout.h>

.section .text

/*
 * SYSENTER entry for the i686 user ABI.
 *
 * ECX and EDX carry the user return EIP and ESP. EBX, ESI, EDI, and EBP carry
 * syscall arguments but remain callee-saved from the inline wrapper's point of
 * view, so their original values are retained beyond the canonical IRET frame.
 */

.extern i486_handle_syscall

.global i686_sysenter_entry
.type i686_sysenter_entry, @function
i686_sysenter_entry:
    /* Values restored after the dispatcher and before IRET. */
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %ebx

    /* Canonical privilege-change frame consumed by IRET. */
    pushl $XINIM_X86_USER_DS_SELECTOR
    pushl %edx
    pushl $XINIM_X86_32_USER_INITIAL_EFLAGS
    pushl $XINIM_X86_USER_CS_SELECTOR
    pushl %ecx

    /*
     * RegisterFrame in reverse field order. SYSENTER reserves ECX and EDX,
     * so the user wrapper passes a1 in ESI, a2 in EDI, and a3 in EBP.
     */
    pushl %gs
    pushl %fs
    pushl %es
    pushl %ds
    pushl $0
    pushl $0
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %ebx
    pushl %eax

    movw $XINIM_X86_KERNEL_DS_SELECTOR, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    cld

    /* Keep IF clear until the frame is canonical and safe to preempt. */
    movl %esp, %ebp
    andl $-16, %esp
    subl $12, %esp
    pushl %ebp
    call i486_handle_syscall
    movl %ebp, %esp

    /* Preserve the return value while restoring user data selectors. */
    movl %eax, XINIM_X86_32_REGISTER_EAX(%esp)
    movw $XINIM_X86_USER_DS_SELECTOR, %cx
    movw %cx, %ds
    movw %cx, %es
    movw %cx, %fs
    movw %cx, %gs

    movl XINIM_X86_32_REGISTER_EAX(%esp), %eax
    addl $XINIM_X86_32_REGISTER_SIZE, %esp

    /* Saved originals follow the five-word IRET frame. */
    movl 20(%esp), %ebx
    movl 24(%esp), %esi
    movl 28(%esp), %edi
    movl 32(%esp), %ebp
    iret

.size i686_sysenter_entry, . - i686_sysenter_entry

.section .note.GNU-stack,"",@progbits
