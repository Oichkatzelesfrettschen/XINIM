#pragma once
#include <cstddef>
#include <cstdint>

namespace xinim::boot {
struct BootInfo;
}

namespace xinim::early {

/// Well-known x86 PC serial port base addresses.
inline constexpr std::uint16_t SERIAL_COM1_BASE = 0x3F8; ///< COM1 (IRQ4)
inline constexpr std::uint16_t SERIAL_COM2_BASE = 0x2F8; ///< COM2 (IRQ3)
inline constexpr std::uint16_t SERIAL_COM3_BASE = 0x3E8; ///< COM3 (IRQ4, shared)
inline constexpr std::uint16_t SERIAL_COM4_BASE = 0x2E8; ///< COM4 (IRQ3, shared)

/// Ring buffer size for interrupt-driven receive (must be power of 2).
inline constexpr std::size_t SERIAL_RX_BUF_SIZE = 256;

class Serial16550 {
  public:
    constexpr explicit Serial16550(std::uint16_t base_port = SERIAL_COM1_BASE) noexcept
        : base_(base_port) {}
    void init();
    void write_char(char c);
    void write(const char* s);
    char read_char();
    [[gnu::no_stack_protector]]
    bool shell(const xinim::boot::BootInfo* boot_info = nullptr,
               bool allow_continue = false);

    /// Enable receive interrupts (IER bit 0). Call after IDT is set up.
    void enable_rx_interrupt();

    /// Called from the ISR when data arrives. Reads the byte and enqueues it.
    void isr_handler();

    /// Non-blocking read: returns true and fills c if data available.
    bool try_read_char(char& c);

    /// Returns the I/O base port (for identifying the source in shared ISR).
    std::uint16_t base_port() const { return base_; }

  private:
    std::uint16_t base_;
    void outb(std::uint16_t port, std::uint8_t val) const;
    std::uint8_t inb(std::uint16_t port) const;

    /// Interrupt-driven receive ring buffer.
    char rx_buf_[SERIAL_RX_BUF_SIZE]{};
    volatile std::size_t rx_head_{0};  ///< ISR writes here
    volatile std::size_t rx_tail_{0};  ///< Consumer reads here
};

} // namespace xinim::early
