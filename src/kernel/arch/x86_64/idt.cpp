#include "idt.hpp"

#include <xinim/abi/x86_segment_selectors.h>

extern "C" void lidt(void *);

namespace xinim::arch::x86_64::idt {

    static IdtEntry g_idt[256];
    static IdtPtr g_idtr;

    static void set_entry(int vec, uint64_t handler, uint8_t type_attr, uint8_t ist) {
        IdtEntry &e = g_idt[vec];
        e.offset_low = handler & 0xFFFF;
        e.selector = XINIM_X86_KERNEL_CS_SELECTOR;
        e.ist = ist & 0x7;
        e.type_attr = type_attr;
        e.offset_mid = (handler >> 16) & 0xFFFF;
        e.offset_high = (handler >> 32) & 0xFFFFFFFF;
        e.zero = 0;
    }

    void set_gate(int vec, void (*handler)(), uint8_t type_attr, uint8_t ist) {
        set_entry(vec, (uint64_t) handler, type_attr, ist);
    }

    void init() {
        g_idtr.limit = sizeof(g_idt) - 1;
        g_idtr.base = (uint64_t) g_idt;
        // zero initialize
        for (int i = 0; i < 256; i++)
            g_idt[i] = IdtEntry{};
        lidt(&g_idtr);
    }

} // namespace xinim::arch::x86_64::idt
