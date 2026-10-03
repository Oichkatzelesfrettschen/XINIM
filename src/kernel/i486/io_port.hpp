#pragma once

#include <stdint.h>

namespace xinim::i486::io_port {

#ifdef XINIM_IO_PORT_TEST
void outb(uint16_t port, uint8_t value) noexcept;
uint8_t inb(uint16_t port) noexcept;
uint16_t inw(uint16_t port) noexcept;
void outw(uint16_t port, uint16_t value) noexcept;
#else
inline void outb(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port) noexcept {
    uint8_t value = 0U;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline uint16_t inw(uint16_t port) noexcept {
    uint16_t value = 0U;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline void outw(uint16_t port, uint16_t value) noexcept {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
#endif

} // namespace xinim::i486::io_port
