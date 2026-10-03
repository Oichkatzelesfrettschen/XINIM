#pragma once
#include <stdint.h>

namespace xinim::arch::x86_64::idt {

struct IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct IdtPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void init();
void set_gate(int vec, void (*handler)(), uint8_t type_attr = 0x8E, uint8_t ist = 0);

}

