/**
 * @file pci.cpp
 * @brief PCI configuration space access helpers.
 */

#include "pci.hpp"
#include <xinim/arch/x86/pci_cfg.hpp>
#include <cstdint>

namespace xinim::hal::x86_64 {

/**
 * @brief Read a DWORD from PCI configuration space.
 *
 * @param bus PCI bus number.
 * @param slot PCI slot number.
 * @param func PCI function number.
 * @param offset Register offset.
 * @return 32-bit register value.
 */
uint32_t Pci::cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return xinim::arch::x86::pci::read_config_dword(bus, slot, func, offset);
}

void* Pci::map_mmio(uint64_t phys_addr) {
    return xinim::arch::x86::pci::map_mmio_physical(phys_addr);
}

} // namespace xinim::hal::x86_64
