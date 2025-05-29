#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"
#include <stdint.h>

#ifdef __x86_64__

/* Configure system call Model Specific Registers. */
void init_syscall_msrs(void)
{
    asm volatile(
        "mov $0xC0000080, %%ecx\n\t"
        "rdmsr\n\t"
        "or $1, %%eax\n\t"
        "wrmsr\n\t"
        "mov $0xC0000081, %%ecx\n\t"
        "mov $(0x18 << 16) | 0x18, %%eax\n\t"
        "xor %%edx, %%edx\n\t"
        "wrmsr\n\t"
        "mov $0xC0000082, %%ecx\n\t"
        "mov $syscall_entry, %%rax\n\t"
        "wrmsr\n\t"
        "mov $0xC0000084, %%ecx\n\t"
        "xor %%eax, %%eax\n\t"
        "xor %%edx, %%edx\n\t"
        "wrmsr"
        :
        :
        : "eax", "ecx", "edx", "memory");
}

/* Entry point for user mode syscalls. */
void syscall_entry(void) __attribute__((naked));
void syscall_entry(void)
{
    __asm__ volatile(
        "call save\n\t"
        "mov %rdi, %rax\n\t"
        "mov %rdx, %rdi\n\t"
        "mov _cur_proc(%%rip), %esi\n\t"
        "mov %rax, %rdx\n\t"
        "mov %rsi, %rcx\n\t"
        "call _sys_call\n\t"
        "jmp restart"
    );
}

#endif

