#ifdef __x86_64__
.text
.global init_syscall_msrs
.global syscall_entry

init_syscall_msrs:
    mov $0xC0000080, %ecx
    rdmsr
    or $1, %eax
    wrmsr
    mov $0xC0000081, %ecx
    mov $(0x18 << 16) | 0x18, %eax
    xor %edx, %edx
    wrmsr
    mov $0xC0000082, %ecx
    mov $syscall_entry, %rax
    wrmsr
    mov $0xC0000084, %ecx
    xor %eax, %eax
    xor %edx, %edx
    wrmsr
    ret

syscall_entry:
    call save
    mov %rdi, %rax
    mov %rdx, %rdi
    mov _cur_proc(%rip), %esi
    mov %rax, %rdx
    mov %rsi, %rcx
    call _sys_call
    jmp _restart
#endif

