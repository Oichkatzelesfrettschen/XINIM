#include "pci.hpp"

namespace xinim::hal::x86_64 {

static inline void outl(uint16_t port, uint32_t val) {
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}
static inline uint32_t inl(uint16_t port) {
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    std::uint32_t v; __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port)); return v;
#else
    (void)port; return 0;
#endif
}

uint32_t Pci::cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    const uint32_t address = (1u<<31) | (static_cast<uint32_t>(bus) << 16)
        | (static_cast<uint32_t>(slot) << 11) | (static_cast<uint32_t>(func) << 8)
        | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

} // namespace xinim::hal::x86_64
