/**
 * @file tss_load.S
 * @brief Assembly code to load TSS
 *
 * Provides low-level LTR instruction to load Task Register.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

.section .text
.code64

/**
 * @brief Load TSS using LTR instruction
 * @param rdi TSS selector (0x28 for entry 5)
 *
 * C signature: extern "C" void tss_load(uint16_t tss_selector);
 *
 * The LTR (Load Task Register) instruction loads the TSS selector
 * into the Task Register, making the TSS active.
 */
.global tss_load
.type tss_load, @function
.align 16
tss_load:
    // Load TSS
    // rdi contains TSS selector (16-bit value)
    ltr di

    // Return to caller
    ret

.size tss_load, . - tss_load
