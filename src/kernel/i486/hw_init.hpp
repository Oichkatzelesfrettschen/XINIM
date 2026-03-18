#pragma once
// Hardware initialization for the i486 ring3 subsystem.

#include "ring3_internal.hpp"

namespace xinim::i486::ring3 {

void set_gdt_entry(int index, uint32_t base, uint32_t limit,
                   uint8_t access, uint8_t granularity) noexcept;
void set_tss_descriptor(int index, uint32_t base, uint32_t limit) noexcept;
void set_idt_gate(uint8_t vector, void (*handler)() noexcept) noexcept;
void set_kernel_fault_gate(uint8_t vector, void (*handler)() noexcept) noexcept;
void set_user_segment_base(uint32_t base) noexcept;
void initialize_protection() noexcept;
void initialize_legacy_pic() noexcept;
void initialize_pit(uint32_t frequency_hz) noexcept;
// Send end-of-interrupt to whichever interrupt controller is active.
// On i486: 8259A PIC EOI.  On i686: Local APIC EOI.
void send_timer_eoi() noexcept;
#ifdef XINIM_ARCH_I686
// Enable CPUID-detected i686 features (SSE, SYSENTER, APIC, IOAPIC) and
// fully mask the legacy 8259A PIC.  Call after initialize_protection().
void initialize_i686_extensions() noexcept;
#endif
void initialize_realtime_clock() noexcept;
[[nodiscard]] uint32_t current_epoch_seconds() noexcept;
[[nodiscard]] uint32_t current_epoch_microseconds() noexcept;

} // namespace xinim::i486::ring3
