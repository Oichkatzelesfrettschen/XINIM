#include <xinim/abi/x86_32_context_layout.h>

.section .text

.global i486_load_gdt
.type i486_load_gdt, @function
i486_load_gdt:
    movl 4(%esp), %eax
    lgdt (%eax)
    ljmp $XINIM_X86_KERNEL_CS_SELECTOR, $.Lflush_segments
.Lflush_segments:
    movw $XINIM_X86_KERNEL_DS_SELECTOR, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret
.size i486_load_gdt, . - i486_load_gdt

.global i486_load_idt
.type i486_load_idt, @function
i486_load_idt:
    movl 4(%esp), %eax
    lidt (%eax)
    ret
.size i486_load_idt, . - i486_load_idt

.global i486_load_tss
.type i486_load_tss, @function
i486_load_tss:
    movw 4(%esp), %ax
    ltr %ax
    ret
.size i486_load_tss, . - i486_load_tss

.global i486_enter_user_mode
.type i486_enter_user_mode, @function
i486_enter_user_mode:
    movl 4(%esp), %eax
    movl 8(%esp), %edx
    movzwl 12(%esp), %ecx
    movzwl 16(%esp), %ebx
    movw %bx, %ds
    movw %bx, %es
    movw %bx, %fs
    movw %bx, %gs
    pushl %ebx
    pushl %edx
    pushfl
    orl $0x200, (%esp)
    pushl %ecx
    pushl %eax
    iret
.size i486_enter_user_mode, . - i486_enter_user_mode

.global i486_resume_user_context
.type i486_resume_user_context, @function
i486_resume_user_context:
    movl 4(%esp), %eax
    jmp i486_restore_user_context
.size i486_resume_user_context, . - i486_resume_user_context

.global i486_switch_to_user_context
.type i486_switch_to_user_context, @function
i486_switch_to_user_context:
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %ebx
    movl 24(%esp), %edx
    movl %esp, (%edx)
    movl 20(%esp), %eax
    jmp i486_restore_user_context
.size i486_switch_to_user_context, . - i486_switch_to_user_context

.global i486_resume_saved_kernel_stack
.type i486_resume_saved_kernel_stack, @function
i486_resume_saved_kernel_stack:
    movl 4(%esp), %eax
    movl %eax, %esp
    popl %ebx
    popl %esi
    popl %edi
    popl %ebp
    ret
.size i486_resume_saved_kernel_stack, . - i486_resume_saved_kernel_stack

.global i486_restore_user_context
.type i486_restore_user_context, @function
i486_restore_user_context:
    pushl XINIM_X86_32_USER_SS(%eax)
    pushl XINIM_X86_32_USER_ESP(%eax)
    pushl XINIM_X86_32_USER_EFLAGS(%eax)
    pushl XINIM_X86_32_USER_CS(%eax)
    pushl XINIM_X86_32_USER_EIP(%eax)
    pushl XINIM_X86_32_USER_GS(%eax)
    pushl XINIM_X86_32_USER_FS(%eax)
    pushl XINIM_X86_32_USER_ES(%eax)
    pushl XINIM_X86_32_USER_DS(%eax)
    pushl XINIM_X86_32_USER_EBP(%eax)
    pushl XINIM_X86_32_USER_EDI(%eax)
    pushl XINIM_X86_32_USER_ESI(%eax)
    pushl XINIM_X86_32_USER_EDX(%eax)
    pushl XINIM_X86_32_USER_ECX(%eax)
    pushl XINIM_X86_32_USER_EBX(%eax)
    pushl XINIM_X86_32_USER_EAX(%eax)
    jmp i486_syscall_return
.size i486_restore_user_context, . - i486_restore_user_context

.extern i486_handle_syscall
.extern i486_handle_timer_irq
.extern i486_handle_fault
.global i486_syscall_entry
.type i486_syscall_entry, @function
i486_syscall_entry:
    pushl %gs
    pushl %fs
    pushl %es
    pushl %ds
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    movw $XINIM_X86_KERNEL_DS_SELECTOR, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    cld
    movl %esp, %ebp
    andl $-16, %esp
    subl $12, %esp
    pushl %ebp
    call i486_handle_syscall
    movl %ebp, %esp
    movl %eax, XINIM_X86_32_REGISTER_EAX(%esp)

i486_syscall_return:
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi
    popl %ebp
    popl %ds
    popl %es
    popl %fs
    popl %gs
    iret
.size i486_syscall_entry, . - i486_syscall_entry

.global i486_timer_irq_entry
.type i486_timer_irq_entry, @function
i486_timer_irq_entry:
    pushl %gs
    pushl %fs
    pushl %es
    pushl %ds
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    movw $XINIM_X86_KERNEL_DS_SELECTOR, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    cld
    movl %esp, %ebp
    andl $-16, %esp
    subl $12, %esp
    pushl %ebp
    call i486_handle_timer_irq

    cli
    hlt
    jmp .
.size i486_timer_irq_entry, . - i486_timer_irq_entry

.macro XINIM_FAULT_STUB_NOERR name, vector
.global \name
.type \name, @function
\name:
    pushl $0
    pushl $\vector
    jmp i486_fault_common
    .size \name, . - \name
.endm

.macro XINIM_FAULT_STUB_ERR name, vector
.global \name
.type \name, @function
\name:
    pushl $\vector
    jmp i486_fault_common
    .size \name, . - \name
.endm

.global i486_fault_common
.type i486_fault_common, @function
i486_fault_common:
    pushl %gs
    pushl %fs
    pushl %es
    pushl %ds
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    movw $XINIM_X86_KERNEL_DS_SELECTOR, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    // Read arguments before realigning the asynchronous kernel stack.
    movl XINIM_X86_32_REGISTER_SIZE(%esp), %eax
    movl XINIM_X86_32_REGISTER_SIZE + 4(%esp), %edx
    movl XINIM_X86_32_REGISTER_SIZE + 8(%esp), %ecx
    andl $-16, %esp
    subl $4, %esp
    pushl %ecx
    pushl %edx
    pushl %eax
    cld
    call i486_handle_fault

    cli
    hlt
    jmp .
.size i486_fault_common, . - i486_fault_common

XINIM_FAULT_STUB_NOERR i486_fault_ud_entry, 6
XINIM_FAULT_STUB_ERR i486_fault_gp_entry, 13
XINIM_FAULT_STUB_ERR i486_fault_pf_entry, 14

.section .note.GNU-stack,"",@progbits
