#pragma once
#include <cstdint>

namespace xinim::early {

class Serial16550 {
  public:
    explicit Serial16550(std::uint16_t base_port = 0x3F8) : base_(base_port) {}
    void init();
    void write_char(char c);
    void write(const char* s);
  private:
    std::uint16_t base_;
    void outb(std::uint16_t port, std::uint8_t val) const;
    std::uint8_t inb(std::uint16_t port) const;
};

} // namespace xinim::early

