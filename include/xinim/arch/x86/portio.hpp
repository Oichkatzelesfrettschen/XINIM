/**
 * @file portio.hpp
 * @brief Shared x86 port I/O primitives (in/out instructions).
 *
 * Used for legacy x86 device access on both 32-bit and 64-bit lanes:
 * PCI config space (0xCF8/0xCFC), serial ports, PIC, PIT, and similar
 * port-mapped devices.
 */
#pragma once

#include <cstdint>

namespace xinim::arch::x86 {

inline uint8_t inb(uint16_t port) noexcept {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

inline uint16_t inw(uint16_t port) noexcept {
    uint16_t val;
    asm volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

inline uint32_t inl(uint16_t port) noexcept {
    uint32_t val;
    asm volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

inline void outb(uint16_t port, uint8_t val) noexcept {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

inline void outw(uint16_t port, uint16_t val) noexcept {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

inline void outl(uint16_t port, uint32_t val) noexcept {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

// I/O delay (read from unused port 0x80 to waste ~1us)
inline void io_wait() noexcept {
    asm volatile("outb %%al, $0x80" : : "a"(static_cast<uint8_t>(0)));
}

} // namespace xinim::arch::x86
