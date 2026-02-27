.section .text
.code64

.globl gdt_load
.type gdt_load, @function
gdt_load:
    # rdi contains the pointer to the GDT descriptor
    lgdt (%rdi)
    
    # Perform a far jump to reload CS
    pushq $0x08                 # New CS (Kernel Code Selector)
    leaq .reload_segments(%rip), %rax
    pushq %rax
    lretq                     # Far return to reload CS

.reload_segments:
    # Reload data segment registers with kernel data selector
    mov $0x10, %ax              # Kernel Data Selector
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
