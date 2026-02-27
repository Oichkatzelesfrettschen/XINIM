.intel_syntax noprefix

.section .text
.code64

.globl tss_load
.type tss_load, @function
.align 16
tss_load:
    # Load TSS selector from rdi
    ltr di
    ret

.size tss_load, . - tss_load
