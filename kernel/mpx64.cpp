#include "../h/const.hpp"
#include "../h/type.hpp"
#include "../include/defs.h"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"

/* 64-bit context switch and interrupt entry code translated from assembly. */

#define RAX_OFF 0
#define RBX_OFF 8
#define RCX_OFF 16
#define RDX_OFF 24
#define RSI_OFF 32
#define RDI_OFF 40
#define RBP_OFF 48
#define R8_OFF 56
#define R9_OFF 64
#define R10_OFF 72
#define R11_OFF 80
#define R12_OFF 88
#define R13_OFF 96
#define R14_OFF 104
#define R15_OFF 112
#define SP_OFF 120
#define PC_OFF 128
#define PSW_OFF 136

extern char k_stack[K_STACK_BYTES];

/*===========================================================================*
 *                              save                                         *
 *===========================================================================*/
/* Save registers to the current process and switch stacks. */
void save(void) NAKED;
void save(void) {
    __asm__ volatile("push %%rax\n\t"
                     "push %%rbx\n\t"
                     "push %%rcx\n\t"
                     "push %%rdx\n\t"
                     "push %%rsi\n\t"
                     "push %%rdi\n\t"
                     "push %%rbp\n\t"
                     "push %%r8\n\t"
                     "push %%r9\n\t"
                     "push %%r10\n\t"
                     "push %%r11\n\t"
                     "push %%r12\n\t"
                     "push %%r13\n\t"
                     "push %%r14\n\t"
                     "push %%r15\n\t"
                     "movq _proc_ptr(%%rip), %%r15\n\t"
                     "movq 120(%%rsp), %%rax\n\t"
                     "movq %%rax, %c0(%%r15)\n\t"
                     "movq 136(%%rsp), %%rax\n\t"
                     "movq %%rax, %c1(%%r15)\n\t"
                     "lea 120(%%rsp), %%rax\n\t"
                     "movq %%rax, %c2(%%r15)\n\t"
                     "movq 112(%%rsp), %%rax\n\t"
                     "movq %%rax, %c3(%%r15)\n\t"
                     "movq 104(%%rsp), %%rax\n\t"
                     "movq %%rax, %c4(%%r15)\n\t"
                     "movq 96(%%rsp), %%rax\n\t"
                     "movq %%rax, %c5(%%r15)\n\t"
                     "movq 88(%%rsp), %%rax\n\t"
                     "movq %%rax, %c6(%%r15)\n\t"
                     "movq 80(%%rsp), %%rax\n\t"
                     "movq %%rax, %c7(%%r15)\n\t"
                     "movq 72(%%rsp), %%rax\n\t"
                     "movq %%rax, %c8(%%r15)\n\t"
                     "movq 64(%%rsp), %%rax\n\t"
                     "movq %%rax, %c9(%%r15)\n\t"
                     "movq 56(%%rsp), %%rax\n\t"
                     "movq %%rax, %c10(%%r15)\n\t"
                     "movq 48(%%rsp), %%rax\n\t"
                     "movq %%rax, %c11(%%r15)\n\t"
                     "movq 40(%%rsp), %%rax\n\t"
                     "movq %%rax, %c12(%%r15)\n\t"
                     "movq 32(%%rsp), %%rax\n\t"
                     "movq %%rax, %c13(%%r15)\n\t"
                     "movq 24(%%rsp), %%rax\n\t"
                     "movq %%rax, %c14(%%r15)\n\t"
                     "movq 16(%%rsp), %%rax\n\t"
                     "movq %%rax, %c15(%%r15)\n\t"
                     "movq 8(%%rsp), %%rax\n\t"
                     "movq %%rax, %c16(%%r15)\n\t"
                     "movq (%%rsp), %%rax\n\t"
                     "movq %%rax, %c17(%%r15)\n\t"
                     "leaq k_stack(%%rip), %%rsp\n\t"
                     "add $%c18, %%rsp\n\t"
                     "ret"
                     :
                     : "i"(PC_OFF), "i"(PSW_OFF), "i"(SP_OFF), "i"(RAX_OFF), "i"(RBX_OFF),
                       "i"(RCX_OFF), "i"(RDX_OFF), "i"(RSI_OFF), "i"(RDI_OFF), "i"(RBP_OFF),
                       "i"(R8_OFF), "i"(R9_OFF), "i"(R10_OFF), "i"(R11_OFF), "i"(R12_OFF),
                       "i"(R13_OFF), "i"(R14_OFF), "i"(R15_OFF), "i"(K_STACK_BYTES)
                     : "memory", "rax", "r15");
}

/*===========================================================================*
 *                              restart                                      *
 *===========================================================================*/
/* Restore registers and continue the interrupted task. */
void restart(void) NAKED;
void restart(void) {
    __asm__ volatile("movq _proc_ptr(%%rip), %%r15\n\t"
                     "movq %c0(%%r15), %%rsp\n\t"
                     "movq %c1(%%r15), %%r15\n\t"
                     "movq %c2(%%r15), %%r14\n\t"
                     "movq %c3(%%r15), %%r13\n\t"
                     "movq %c4(%%r15), %%r12\n\t"
                     "movq %c5(%%r15), %%r11\n\t"
                     "movq %c6(%%r15), %%r10\n\t"
                     "movq %c7(%%r15), %%r9\n\t"
                     "movq %c8(%%r15), %%r8\n\t"
                     "movq %c9(%%r15), %%rbp\n\t"
                     "movq %c10(%%r15), %%rdi\n\t"
                     "movq %c11(%%r15), %%rsi\n\t"
                     "movq %c12(%%r15), %%rdx\n\t"
                     "movq %c13(%%r15), %%rcx\n\t"
                     "movq %c14(%%r15), %%rbx\n\t"
                     "movq %c15(%%r15), %%rax\n\t"
                     "pushq %c16(%%r15)\n\t"
                     "pushq %c17(%%r15)\n\t"
                     "iretq"
                     :
                     : "i"(SP_OFF), "i"(R15_OFF), "i"(R14_OFF), "i"(R13_OFF), "i"(R12_OFF),
                       "i"(R11_OFF), "i"(R10_OFF), "i"(R9_OFF), "i"(R8_OFF), "i"(RBP_OFF),
                       "i"(RDI_OFF), "i"(RSI_OFF), "i"(RDX_OFF), "i"(RCX_OFF), "i"(RBX_OFF),
                       "i"(RAX_OFF), "i"(PSW_OFF), "i"(PC_OFF)
                     : "memory", "rax", "r15");
}

/*===========================================================================*
 *                              isr_default                                  *
 *===========================================================================*/
/* Default interrupt service routine. */
void isr_default(void) NAKED;
void isr_default(void) {
    __asm__ volatile("call save\n\t"
                     "call _surprise\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              isr_clock                                    *
 *===========================================================================*/
/* Clock interrupt service routine. */
void isr_clock(void) NAKED;
void isr_clock(void) {
    __asm__ volatile("call save\n\t"
                     "call _clock_int\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              isr_keyboard                                 *
 *===========================================================================*/
/* Keyboard interrupt service routine. */
void isr_keyboard(void) NAKED;
void isr_keyboard(void) {
    __asm__ volatile("call save\n\t"
                     "call _tty_int\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              s_call                                       *
 *===========================================================================*/
/* System call entry point. */
void s_call(void) NAKED;
void s_call(void) {
    __asm__ volatile("call save\n\t"
                     "movq _proc_ptr(%rip), %rdi\n\t"
                     "movq 16(%rdi), %rsi\n\t"
                     "movq (%rdi), %rdx\n\t"
                     "movq $0, %rcx\n\t"
                     "call _sys_call\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              lpr_int                                      *
 *===========================================================================*/
/* Printer interrupt service routine. */
void lpr_int(void) NAKED;
void lpr_int(void) {
    __asm__ volatile("call save\n\t"
                     "call _pr_char\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              disk_int                                     *
 *===========================================================================*/
/* Disk interrupt service routine. */
void disk_int(void) NAKED;
void disk_int(void) {
    __asm__ volatile("call save\n\t"
                     "movq _int_mess+2(%rip), %rax\n\t"
                     "movq %rax, %rdi\n\t"
                     "call _interrupt\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              divide                                       *
 *===========================================================================*/
/* Divide trap handler. */
void divide(void) NAKED;
void divide(void) {
    __asm__ volatile("call save\n\t"
                     "call _div_trap\n\t"
                     "jmp restart");
}

/*===========================================================================*
 *                              trp                                          *
 *===========================================================================*/
/* General trap handler. */
void trp(void) NAKED;
void trp(void) {
    __asm__ volatile("call save\n\t"
                     "call _trap\n\t"
                     "jmp restart");
}
