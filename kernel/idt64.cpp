#include "../include/defs.hpp" // Provides uN_t types
#include "const.hpp"
#include "type.hpp"
#include <cstddef> // For std::size_t, nullptr (though not directly used for ptr)
#include <cstdint> // For standard uintN_t types (preferred over uN_t directly)

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

// Modernized declarations for external assembly ISRs
extern void isr_default() noexcept;
extern void isr_clock() noexcept;
extern void isr_keyboard() noexcept;

/**
 * @brief Fill one IDT entry with a handler.
 *
 * @param n       Interrupt vector number.
 * @param handler Handler routine address.
 * @param ist     Interrupt stack table index.
 */
static void idt_set_gate(int n, void (*handler)() noexcept, unsigned int ist) noexcept {
    // Casting function pointer to uint64_t for address manipulation
    uint64_t addr = reinterpret_cast<uint64_t>(reinterpret_cast<uintptr_t>(handler));
    idt[n].offset_low = static_cast<uint16_t>(addr & 0xFFFF);              // u16_t
    idt[n].selector = 0x08; /* kernel code segment */                      // u16_t
    idt[n].ist = static_cast<uint8_t>(ist & 0x7);                          // u8_t
    idt[n].type_attr = 0x8E; /* interrupt gate */                          // u8_t
    idt[n].offset_mid = static_cast<uint16_t>((addr >> 16) & 0xFFFF);      // u16_t
    idt[n].offset_high = static_cast<uint32_t>((addr >> 32) & 0xFFFFFFFF); // u32_t
    idt[n].zero = 0;                                                       // u32_t
}

/**
 * @brief Initialize the 64-bit IDT and TSS.
 */
void idt_init() noexcept {
    int i;

    /* Set up interrupt stack in TSS.  IST1 is used for all interrupts. */
    // int_stack is u8_t[]. Adding size gives pointer to one past the end.
    kernel_tss.ist1 = reinterpret_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(int_stack + sizeof(int_stack)));      // u64_t
    kernel_tss.io_map_base = static_cast<uint16_t>(sizeof(struct tss64)); // u16_t

    for (i = 0; i < 256; i++)
        idt_set_gate(i, isr_default, 1); // isr_default is noexcept

    idt_set_gate(CLOCK_VECTOR, isr_clock, 1);       // isr_clock is noexcept
    idt_set_gate(KEYBOARD_VECTOR, isr_keyboard, 1); // isr_keyboard is noexcept

    idt_desc.limit = static_cast<uint16_t>(sizeof(idt) - 1);                      // u16_t
    idt_desc.base = reinterpret_cast<uint64_t>(reinterpret_cast<uintptr_t>(idt)); // u64_t

    asm volatile("lidt %0" : : "m"(idt_desc));
    asm volatile("ltr %%ax" : : "a"(0x28)); /* TSS descriptor is at GDT entry 5 */
}
