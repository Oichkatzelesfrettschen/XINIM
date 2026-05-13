#include "hw_init.hpp"

#ifdef XINIM_ARCH_I686
#include "../i686/cpuid.hpp"
#include "../i686/sse.hpp"
#include "../i686/sysenter.hpp"
#include "../i686/apic.hpp"
#include "../i686/ioapic.hpp"
#include "../i686/irq_stubs.hpp"
#endif

namespace xinim::i486::ring3 {

#if defined(XINIM_ARCH_I686) && defined(XINIM_I686_APIC_TIMER)
namespace {
bool g_timer_uses_apic = false;
}
#endif

void set_gdt_entry(int index,
                   uint32_t base,
                   uint32_t limit,
                   uint8_t access,
                   uint8_t granularity) noexcept {
    g_gdt[index].limit_low = static_cast<uint16_t>(limit & 0xFFFFU);
    g_gdt[index].base_low = static_cast<uint16_t>(base & 0xFFFFU);
    g_gdt[index].base_middle = static_cast<uint8_t>((base >> 16U) & 0xFFU);
    g_gdt[index].access = access;
    g_gdt[index].granularity =
        static_cast<uint8_t>(((limit >> 16U) & 0x0FU) | (granularity & 0xF0U));
    g_gdt[index].base_high = static_cast<uint8_t>((base >> 24U) & 0xFFU);
}

void set_tss_descriptor(int index, uint32_t base, uint32_t limit) noexcept {
    set_gdt_entry(index, base, limit, 0x89U, 0x00U);
    g_gdt[index + 1] = {};
}

void set_idt_gate(uint8_t vector, void (*handler)() noexcept) noexcept {
    const uint32_t address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handler));
    g_idt[vector].offset_low = static_cast<uint16_t>(address & 0xFFFFU);
    g_idt[vector].selector = kKernelCodeSelector;
    g_idt[vector].zero = 0U;
    g_idt[vector].type_attr = 0xEEU;
    g_idt[vector].offset_high = static_cast<uint16_t>((address >> 16U) & 0xFFFFU);
}

void set_kernel_fault_gate(uint8_t vector, void (*handler)() noexcept) noexcept {
    const uint32_t address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handler));
    g_idt[vector].offset_low = static_cast<uint16_t>(address & 0xFFFFU);
    g_idt[vector].selector = kKernelCodeSelector;
    g_idt[vector].zero = 0U;
    g_idt[vector].type_attr = 0x8EU;
    g_idt[vector].offset_high = static_cast<uint16_t>((address >> 16U) & 0xFFFFU);
}

void set_user_segment_base(uint32_t base) noexcept {
    const uint32_t limit_pages =
        ((elf32::kUserVirtualBase + elf32::kUserAddressSpaceSize) / kPageSize) - 1U;
    set_gdt_entry(3, base, limit_pages, 0xFAU, 0xC0U);
    set_gdt_entry(4, base, limit_pages, 0xF2U, 0xC0U);
}

void initialize_protection() noexcept {
    set_gdt_entry(0, 0U, 0U, 0U, 0U);
    set_gdt_entry(1, 0U, 0x000FFFFFU, 0x9AU, 0xCFU);
    set_gdt_entry(2, 0U, 0x000FFFFFU, 0x92U, 0xCFU);
    set_user_segment_base(0U);

    g_tss = {};
    g_tss.ss0 = kKernelDataSelector;
    g_tss.esp0 = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(g_bootstrap_kernel_stack + sizeof(g_bootstrap_kernel_stack)));
    g_tss.iomap_base = sizeof(TssEntry);
    set_tss_descriptor(5,
                       static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_tss)),
                       static_cast<uint32_t>(sizeof(TssEntry) - 1U));

    const GdtDescriptor gdt_descriptor{
        static_cast<uint16_t>(sizeof(g_gdt) - 1U),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_gdt)),
    };
    i486_load_gdt(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gdt_descriptor)));
    i486_load_tss(kTssSelector);

    for (auto& entry : g_idt) {
        entry = {};
    }
    set_kernel_fault_gate(kTimerVector, i486_timer_irq_entry);
    set_idt_gate(kSyscallVector, i486_syscall_entry);
    set_kernel_fault_gate(6U, i486_fault_ud_entry);
    set_kernel_fault_gate(13U, i486_fault_gp_entry);
    set_kernel_fault_gate(14U, i486_fault_pf_entry);
    const IdtDescriptor idt_descriptor{
        static_cast<uint16_t>(sizeof(g_idt) - 1U),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_idt)),
    };
    i486_load_idt(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&idt_descriptor)));
}

void initialize_legacy_pic() noexcept {
    outb(kPic1DataPort, 0xFFU);
    outb(kPic2DataPort, 0xFFU);

    outb(kPic1CommandPort, kPicInitialize);
    outb(kPic2CommandPort, kPicInitialize);
    outb(kPic1DataPort, kTimerVector);
    outb(kPic2DataPort, static_cast<uint8_t>(kTimerVector + 8U));
    outb(kPic1DataPort, 0x04U);
    outb(kPic2DataPort, 0x02U);
    outb(kPic1DataPort, kPic8086Mode);
    outb(kPic2DataPort, kPic8086Mode);

    outb(kPic1DataPort, 0xFEU);
    outb(kPic2DataPort, 0xFFU);
}

void initialize_pit(uint32_t frequency_hz) noexcept {
    const uint32_t divisor = frequency_hz == 0U ? 0U : (kPitInputHz / frequency_hz);
    outb(kPitModePort, 0x36U);
    outb(kPitChannel0Port, static_cast<uint8_t>(divisor & 0xFFU));
    outb(kPitChannel0Port, static_cast<uint8_t>((divisor >> 8U) & 0xFFU));
}

void send_timer_eoi() noexcept {
#if defined(XINIM_ARCH_I686) && defined(XINIM_I686_APIC_TIMER)
    if (g_timer_uses_apic) {
        xinim::i686::apic::send_apic_eoi();
        return;
    }
    outb(kPic1CommandPort, kPicEoi);
#else
    outb(kPic1CommandPort, kPicEoi);
#endif
}

#ifdef XINIM_ARCH_I686
void initialize_i686_extensions() noexcept {
    // CPUID: detect available hardware features.
    xinim::i686::initialize_cpuid();
    const auto& feat = xinim::i686::cpu_features();

    // SSE: enable OSFXSR + OSXMMEXCPT in CR4 so FXSAVE/FXRESTORE work.
    if (feat.has_fxsr) {
        xinim::i686::sse::initialize_sse();
    }

    // The APIC timer path is still experimental in the QEMU pc/pentium3 lane.
    // Default to legacy PIC/PIT so blocking TTY reads keep getting preempted
    // and COM2 input can wake the supervised shell.  Enable
    // XINIM_I686_APIC_TIMER only when actively debugging APIC routing.
#ifdef XINIM_I686_APIC_TIMER
    xinim::i686::apic::initialize_apic();
    xinim::i686::apic::initialize_apic_timer();
    g_timer_uses_apic = true;

    // IOAPIC: route IRQ1 (keyboard) and IRQ14 (IDE); mask everything else.
    xinim::i686::ioapic::initialize_ioapic();
#else
    initialize_legacy_pic();
    initialize_pit(kTimerHz);
#endif

    // SYSENTER: program MSRs 0x174/0x175/0x176 for fast syscall path.
    if (feat.has_sysenter) {
        xinim::i686::sysenter::initialize_sysenter();
    }

    // Install IDT stubs for APIC-delivered vectors that have no full handler.
    //
    // 0xFF -- Spurious APIC interrupt: iret with no EOI (SDM Vol. 3A 10.9).
    //         Required because the APIC spurious vector is always enabled.
    //
    // 0x21 -- IOAPIC IRQ1 (keyboard): EOI stub (keyboard uses polling).
    // 0x2E -- IOAPIC IRQ14 (IDE):     EOI stub (IDE uses polling).
    //
    // Without these, the CPU would see a null IDT gate and triple-fault on
    // the first spurious APIC interrupt or keypress.
#ifdef XINIM_I686_APIC_TIMER
    set_kernel_fault_gate(0xFFU, xinim::i686::i686_apic_spurious_entry);
    set_kernel_fault_gate(xinim::i686::ioapic::kVectorKeyboard,
                          xinim::i686::i686_apic_irq_eoi_entry);
    set_kernel_fault_gate(xinim::i686::ioapic::kVectorIdePri,
                          xinim::i686::i686_apic_irq_eoi_entry);

    // Fully mask both 8259A PIC chips now that the IOAPIC handles routing.
    // WHY: leaving PIC unmasked after enabling APIC causes spurious IRQ 7/15
    // on some chipsets where both sources deliver to the CPU simultaneously.
    outb(kPic1DataPort, 0xFFU);
    outb(kPic2DataPort, 0xFFU);
#endif
}
#endif

// -- RTC and time support --------------------------------------------------

namespace {

uint8_t read_cmos(uint8_t reg) noexcept {
    outb(0x70U, reg);
    return inb_port(0x71U);
}

uint8_t bcd_to_bin(uint8_t bcd) noexcept {
    return static_cast<uint8_t>((bcd >> 4U) * 10U + (bcd & 0x0FU));
}

uint32_t days_in_months(uint32_t month, bool leap) noexcept {
    static constexpr uint16_t kCum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month > 11U) month = 11U;
    uint32_t days = kCum[month];
    if (leap && month >= 2U) ++days;
    return days;
}

uint32_t read_rtc_epoch() noexcept {
    while ((read_cmos(0x0AU) & 0x80U) != 0U) {}
    uint8_t sec = bcd_to_bin(read_cmos(0x00U));
    uint8_t min = bcd_to_bin(read_cmos(0x02U));
    uint8_t hour = bcd_to_bin(read_cmos(0x04U));
    uint8_t day = bcd_to_bin(read_cmos(0x07U));
    uint8_t month = bcd_to_bin(read_cmos(0x08U));
    uint8_t year_bcd = bcd_to_bin(read_cmos(0x09U));
    uint32_t year = 2000U + static_cast<uint32_t>(year_bcd);

    uint32_t days = 0U;
    for (uint32_t y = 1970U; y < year; ++y) {
        bool leap = (y % 4U == 0U && (y % 100U != 0U || y % 400U == 0U));
        days += leap ? 366U : 365U;
    }
    bool cur_leap = (year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U));
    if (month > 0U) days += days_in_months(month - 1U, cur_leap);
    if (day > 0U) days += day - 1U;

    return days * 86400U + static_cast<uint32_t>(hour) * 3600U +
           static_cast<uint32_t>(min) * 60U + static_cast<uint32_t>(sec);
}

} // namespace

void initialize_realtime_clock() noexcept {
    g_boot_epoch_seconds = read_rtc_epoch();
    g_boot_epoch_ticks = g_scheduler_ticks;
}

uint32_t current_epoch_seconds() noexcept {
    const uint64_t elapsed_ticks = g_scheduler_ticks - g_boot_epoch_ticks;
    return g_boot_epoch_seconds + static_cast<uint32_t>(elapsed_ticks / kTimerHz);
}

uint32_t current_epoch_microseconds() noexcept {
    const uint64_t elapsed_ticks = g_scheduler_ticks - g_boot_epoch_ticks;
    const uint32_t sub_second_ticks = static_cast<uint32_t>(elapsed_ticks % kTimerHz);
    return sub_second_ticks * (1000000U / kTimerHz);
}

} // namespace xinim::i486::ring3
