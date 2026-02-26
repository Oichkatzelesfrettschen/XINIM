/**
 * @file pci.cpp
 * @brief PCI configuration space access helpers.
 */

#include "pci.hpp"
#include <cstdint>

namespace xinim::hal::x86_64 {

/**
 * @brief Write a 32-bit value to an I/O port.
 *
 * @param port Port number.
 * @param val Value to write.
 */
static inline void outl(uint16_t port, uint32_t val) {
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}
/**
 * @brief Read a 32-bit value from an I/O port.
 *
 * @param port Port number.
 * @return 32-bit value.
 */
static inline uint32_t inl(uint16_t port) {
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    std::uint32_t v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
#else
    (void)port; return 0;
#endif
}

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
    const uint32_t address = (1u<<31) | (static_cast<uint32_t>(bus) << 16)
        | (static_cast<uint32_t>(slot) << 11) | (static_cast<uint32_t>(func) << 8)
        | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

} // namespace xinim::hal::x86_64
