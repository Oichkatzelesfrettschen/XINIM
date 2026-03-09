.section .text

.global i486_load_gdt
.type i486_load_gdt, @function
i486_load_gdt:
    movl 4(%esp), %eax
    lgdt (%eax)
    ljmp $0x08, $.Lflush_segments
.Lflush_segments:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret

.global i486_load_idt
.type i486_load_idt, @function
i486_load_idt:
    movl 4(%esp), %eax
    lidt (%eax)
    ret

.global i486_load_tss
.type i486_load_tss, @function
i486_load_tss:
    movw 4(%esp), %ax
    ltr %ax
    ret

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

.global i486_resume_user_context
.type i486_resume_user_context, @function
i486_resume_user_context:
    movl 4(%esp), %eax
    jmp i486_restore_user_context

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

.global i486_restore_user_context
.type i486_restore_user_context, @function
i486_restore_user_context:
    pushl 60(%eax)
    pushl 56(%eax)
    pushl 52(%eax)
    pushl 48(%eax)
    pushl 44(%eax)
    pushl 40(%eax)
    pushl 36(%eax)
    pushl 32(%eax)
    pushl 28(%eax)
    pushl 24(%eax)
    pushl 20(%eax)
    pushl 16(%eax)
    pushl 12(%eax)
    pushl 8(%eax)
    pushl 4(%eax)
    pushl 0(%eax)
    jmp i486_syscall_return

.extern i486_handle_syscall
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

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %esp
    call i486_handle_syscall
    addl $4, %esp
    movl %eax, (%esp)

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

.macro XINIM_FAULT_STUB_NOERR name, vector
.global \name
.type \name, @function
\name:
    pushl $0
    pushl $\vector
    jmp i486_fault_common
.endm

.macro XINIM_FAULT_STUB_ERR name, vector
.global \name
.type \name, @function
\name:
    pushl $\vector
    jmp i486_fault_common
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

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movl 44(%esp), %eax
    movl 48(%esp), %edx
    pushl %edx
    pushl %eax
    call i486_handle_fault

    cli
    hlt
    jmp .

XINIM_FAULT_STUB_NOERR i486_fault_ud_entry, 6
XINIM_FAULT_STUB_ERR i486_fault_gp_entry, 13
XINIM_FAULT_STUB_ERR i486_fault_pf_entry, 14
