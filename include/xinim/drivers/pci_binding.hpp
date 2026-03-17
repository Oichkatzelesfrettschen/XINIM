/**
 * @file pci_binding.hpp
 * @brief Header-only helpers for binding PCI BARs into drivers.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <xinim/arch/x86/mmio.hpp>
#include <xinim/pci/pci.hpp>

namespace xinim::drivers::pci_binding {

struct MappedBar {
    volatile uint8_t* base{nullptr};
    uint64_t physical{0};
    uint64_t size{0};
    uint8_t index{0};
    bool is_64bit{false};
    bool is_prefetchable{false};

    [[nodiscard]] bool valid() const noexcept {
        return base != nullptr && size != 0U;
    }
};

inline bool bind_mmio_bar(const xinim::pci::PCIDevice& device,
                          uint8_t bar_index,
                          MappedBar& out) noexcept {
    out = {};

    if (!device.is_valid() || bar_index >= 6U) {
        return false;
    }

    const xinim::pci::BAR& bar = device.bars[bar_index];
    if (!bar.is_valid() || !bar.is_mmio) {
        return false;
    }

    out.base = xinim::arch::x86::mmio::map_pointer<volatile uint8_t>(bar.address);
    out.physical = bar.address;
    out.size = bar.size;
    out.index = bar_index;
    out.is_64bit = bar.is_64bit;
    out.is_prefetchable = bar.is_prefetchable;
    return out.valid();
}

} // namespace xinim::drivers::pci_binding
