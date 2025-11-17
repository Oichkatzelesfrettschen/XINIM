/**
 * @file gdt_load.S
 * @brief Assembly code to load GDT
 *
 * Provides low-level LGDT instruction and segment register reloading.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

.section .text
.code64

/**
 * @brief Load GDT and reload segment registers
 * @param rdi Pointer to GDT pointer structure
 *
 * C signature: extern "C" void gdt_load(uint64_t gdt_ptr_address);
 *
 * This function:
 * 1. Loads the GDT using LGDT instruction
 * 2. Reloads code segment (CS) using far return
 * 3. Reloads data segments (DS, ES, FS, GS, SS)
 */
.global gdt_load
.type gdt_load, @function
.align 16
gdt_load:
    // Load GDT
    // rdi contains address of GdtPointer structure
    lgdt (%rdi)

    // Reload code segment using far return
    // We can't directly mov to CS, must use far return or far jump
    // Push kernel code selector (0x08)
    push $0x08
    // Push return address (next instruction)
    lea rax, [rip + .reload_segments]
    push rax
    // Far return (pops CS:RIP from stack)
    lretq

.reload_segments:
    // Now CS is reloaded, reload data segments
    mov ax, 0x10    // Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    // Return to caller
    ret

.size gdt_load, . - gdt_load
