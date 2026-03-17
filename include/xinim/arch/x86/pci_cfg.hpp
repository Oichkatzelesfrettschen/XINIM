/**
 * @file pci_cfg.hpp
 * @brief Shared x86 PCI configuration and MMIO mapping helpers.
 */
#pragma once

#include <cstdint>
#include <cstddef>

#include <xinim/arch/x86/mmio.hpp>
#include <xinim/arch/x86/portio.hpp>

namespace xinim::arch::x86::pci {

constexpr uint16_t kConfigAddressPort = 0xCF8;
constexpr uint16_t kConfigDataPort = 0xCFC;

inline uint32_t config_address(uint8_t bus,
                               uint8_t device,
                               uint8_t function,
                               uint16_t offset) noexcept {
    return 0x80000000U |
           (static_cast<uint32_t>(bus) << 16U) |
           (static_cast<uint32_t>(device) << 11U) |
           (static_cast<uint32_t>(function) << 8U) |
           (static_cast<uint32_t>(offset) & 0xFCU);
}

inline uint8_t read_config_byte(uint8_t bus,
                                uint8_t device,
                                uint8_t function,
                                uint16_t offset) noexcept {
    x86::outl(kConfigAddressPort, config_address(bus, device, function, offset));
    return x86::inb(static_cast<uint16_t>(kConfigDataPort + (offset & 3U)));
}

inline uint16_t read_config_word(uint8_t bus,
                                 uint8_t device,
                                 uint8_t function,
                                 uint16_t offset) noexcept {
    x86::outl(kConfigAddressPort, config_address(bus, device, function, offset));
    return x86::inw(static_cast<uint16_t>(kConfigDataPort + (offset & 2U)));
}

inline uint32_t read_config_dword(uint8_t bus,
                                  uint8_t device,
                                  uint8_t function,
                                  uint16_t offset) noexcept {
    x86::outl(kConfigAddressPort, config_address(bus, device, function, offset));
    return x86::inl(kConfigDataPort);
}

inline void write_config_byte(uint8_t bus,
                              uint8_t device,
                              uint8_t function,
                              uint16_t offset,
                              uint8_t value) noexcept {
    x86::outl(kConfigAddressPort, config_address(bus, device, function, offset));
    x86::outb(static_cast<uint16_t>(kConfigDataPort + (offset & 3U)), value);
}

inline void write_config_word(uint8_t bus,
                              uint8_t device,
                              uint8_t function,
                              uint16_t offset,
                              uint16_t value) noexcept {
    x86::outl(kConfigAddressPort, config_address(bus, device, function, offset));
    x86::outw(static_cast<uint16_t>(kConfigDataPort + (offset & 2U)), value);
}

inline void write_config_dword(uint8_t bus,
                               uint8_t device,
                               uint8_t function,
                               uint16_t offset,
                               uint32_t value) noexcept {
    x86::outl(kConfigAddressPort, config_address(bus, device, function, offset));
    x86::outl(kConfigDataPort, value);
}

inline void* map_mmio_physical(uint64_t phys_addr) noexcept {
    return mmio::map_pointer(phys_addr);
}

inline void unmap_mmio([[maybe_unused]] void* mapped_address,
                       [[maybe_unused]] size_t size) noexcept {
    // Shared x86 callers currently use direct physical/HHDM mappings only.
}

} // namespace xinim::arch::x86::pci
