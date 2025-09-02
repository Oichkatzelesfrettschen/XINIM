#include "serial_16550.hpp"

namespace xinim::early {

[[maybe_unused]] static inline void io_wait() {
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
#endif
}

void Serial16550::outb(std::uint16_t port, std::uint8_t val) const {
    (void)port; (void)val;
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
#endif
}

std::uint8_t Serial16550::inb(std::uint16_t port) const {
    std::uint8_t ret;
    (void)port; ret = 0;
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
#endif
    return ret;
}

void Serial16550::init() {
    outb(base_ + 1, 0x00); // Disable all interrupts
    outb(base_ + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(base_ + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(base_ + 1, 0x00); //                  (hi byte)
    outb(base_ + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(base_ + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
    outb(base_ + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void Serial16550::write_char(char c) {
    while ((inb(base_ + 5) & 0x20) == 0) { /* wait for THR empty */ }
    outb(base_, static_cast<std::uint8_t>(c));
}

void Serial16550::write(const char* s) {
    for (; *s; ++s) {
        if (*s == '\n') write_char('\r');
        write_char(*s);
    }
}

} // namespace xinim::early
