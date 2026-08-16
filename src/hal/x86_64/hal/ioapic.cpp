/**
 * @file ioapic.cpp
 * @brief I/O APIC register access implementation.
 *
 * Provides minimal IOAPIC programming for interrupt redirection.
 */

#include "ioapic.hpp"

namespace xinim::hal::x86_64 {

    /**
     * @brief Initialize IOAPIC MMIO base and GSI offset.
     *
     * @param mmio_base Physical MMIO base address.
     * @param gsi_base Base global system interrupt number.
     */
    void IoApic::init(uintptr_t mmio_base, uint32_t gsi_base) {
        base_ = reinterpret_cast<volatile uint32_t *>(mmio_base);
        gsi_base_ = gsi_base;
        entry_count_ = ((read(0x01U) >> 16U) & 0xffU) + 1U;
    }

    /**
     * @brief Write an IOAPIC register.
     *
     * @param reg Register index.
     * @param value Value to write.
     */
    void IoApic::write(uint8_t reg, uint32_t value) {
        base_[0] = reg;
        base_[4] = value;
    }

    /**
     * @brief Read an IOAPIC register.
     *
     * @param reg Register index.
     * @return Register value.
     */
    uint32_t IoApic::read(uint8_t reg) {
        base_[0] = reg;
        return base_[4];
    }

    /**
     * @brief Program a redirection entry for a given GSI.
     *
     * @param gsi Global system interrupt.
     * @param vector Interrupt vector to deliver.
     * @param level True for level-triggered, false for edge-triggered.
     * @param active_low True for active-low polarity.
     */
    bool IoApic::redirect(uint32_t gsi, uint8_t vector, bool level, bool active_low) {
        if (base_ == nullptr || gsi < gsi_base_ || gsi - gsi_base_ >= entry_count_) {
            return false;
        }
        const uint32_t idx_raw = (gsi - gsi_base_) * 2u + 0x10u;
        const auto idx = static_cast<uint8_t>(idx_raw);
        uint32_t low = vector;
        if (active_low)
            low |= (1u << 13);
        if (level)
            low |= (1u << 15);
        // Unmask (bit 16 = 0)
        write(idx, low);
        write(static_cast<uint8_t>(idx + 1u), 0u); // destination CPU ID 0
        return true;
    }

} // namespace xinim::hal::x86_64
