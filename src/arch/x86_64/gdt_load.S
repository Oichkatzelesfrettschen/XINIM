#include <xinim/abi/x86_segment_selectors.h>

.section .text
.code64

.globl gdt_load
.type gdt_load, @function
gdt_load:
    # rdi contains the pointer to the GDT descriptor
    lgdt (%rdi)
    
    # Perform a far jump to reload CS
    pushq $XINIM_X86_KERNEL_CS_SELECTOR
    leaq .reload_segments(%rip), %rax
    pushq %rax
    lretq                     # Far return to reload CS

.reload_segments:
    # Reload data segment registers with kernel data selector
    mov $XINIM_X86_KERNEL_DS_SELECTOR, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    # Set FS and GS to null selector (0) to preserve their hidden base
    # registers. The kernel uses GS.base for per-CPU data via WRMSR;
    # loading a non-null selector would clear the base to 0.
    xor %ax, %ax
    mov %ax, %fs
    mov %ax, %gs
    ret

.size gdt_load, .-gdt_load

.section .note.GNU-stack,"",@progbits
