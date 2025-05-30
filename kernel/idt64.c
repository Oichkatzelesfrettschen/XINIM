#include "../include/defs.h"
#include "const.hpp"
#include "type.hpp"

/* 64-bit Interrupt Descriptor Table and simple TSS setup.  This replaces the
 * real mode interrupt vector copying done on the 8086 version.  The code only
 * allocates the tables and loads them; the individual interrupt handlers are
 * still implemented in assembly.
 */

struct idt_entry {
    u16_t offset_low;
    u16_t selector;
    u8_t ist;
    u8_t type_attr;
    u16_t offset_mid;
    u32_t offset_high;
    u32_t zero;
} PACKED;

struct idt_ptr {
    u16_t limit;
    u64_t base;
} PACKED;

struct tss64 {
    u32_t reserved0;
    u64_t rsp0;
    u64_t rsp1;
    u64_t rsp2;
    u64_t reserved1;
    u64_t ist1;
    u64_t ist2;
    u64_t ist3;
    u64_t ist4;
    u64_t ist5;
    u64_t ist6;
    u64_t ist7;
    u64_t reserved2;
    u16_t reserved3;
    u16_t io_map_base;
} PACKED;

/* Single shared interrupt stack. */
static u8_t int_stack[4096];

static struct tss64 kernel_tss;
static struct idt_entry idt[256];
static struct idt_ptr idt_desc;

extern void isr_default(void);
extern void isr_clock(void);
extern void isr_keyboard(void);

/*===========================================================================*
 *                              idt_set_gate                                 *
 *===========================================================================*/
/* Fill one entry of the IDT.
 * n: interrupt vector number.
 * handler: address of handler routine.
 * ist: interrupt stack table index.
 */
static void idt_set_gate(int n, void (*handler)(), unsigned ist) {
    u64_t addr = (u64_t)handler;
    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x08; /* kernel code segment */
    idt[n].ist = ist & 0x7;
    idt[n].type_attr = 0x8E; /* interrupt gate */
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

/*===========================================================================*
 *                              idt_init                                     *
 *===========================================================================*/
/* Initialize the 64-bit IDT and TSS. */
void idt_init(void) {
    int i;

    /* Set up interrupt stack in TSS.  IST1 is used for all interrupts. */
    kernel_tss.ist1 = (u64_t)(int_stack + sizeof(int_stack));
    kernel_tss.io_map_base = sizeof(struct tss64);

    for (i = 0; i < 256; i++)
        idt_set_gate(i, isr_default, 1);

    idt_set_gate(CLOCK_VECTOR, isr_clock, 1);
    idt_set_gate(KEYBOARD_VECTOR, isr_keyboard, 1);

    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (u64_t)idt;

    asm volatile("lidt %0" : : "m"(idt_desc));
    asm volatile("ltr %%ax" : : "a"(0x28)); /* TSS descriptor is at GDT entry 5 */
}
