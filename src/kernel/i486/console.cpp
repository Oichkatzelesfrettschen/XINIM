#include "console.hpp"

namespace xinim::i486::console {
namespace {

constexpr uint16_t kCom1 = 0x3F8U;
constexpr uint16_t kCom2 = 0x2F8U;
constexpr uint16_t kVgaWidth = 80U;
constexpr uint16_t kVgaHeight = 25U;
constexpr uint8_t kVgaColor = 0x0FU;

volatile uint16_t* const g_vga = reinterpret_cast<volatile uint16_t*>(0xB8000U);
uint16_t g_cursor_x = 0U;
uint16_t g_cursor_y = 0U;

inline void outb(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port) noexcept {
    uint8_t value = 0U;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_initialize(uint16_t port) noexcept {
    outb(port + 1U, 0x00U);
    outb(port + 3U, 0x80U);
    outb(port + 0U, 0x03U);
    outb(port + 1U, 0x00U);
    outb(port + 3U, 0x03U);
    outb(port + 2U, 0xC7U);
    outb(port + 4U, 0x0BU);
}

void serial_write(uint16_t port, char c) noexcept {
    while ((inb(port + 5U) & 0x20U) == 0U) {
    }
    outb(port, static_cast<uint8_t>(c));
}

char serial_read(uint16_t port) noexcept {
    while ((inb(port + 5U) & 0x01U) == 0U) {
    }
    return static_cast<char>(inb(port));
}

void update_cursor() noexcept {
    const uint16_t position = static_cast<uint16_t>(g_cursor_y * kVgaWidth + g_cursor_x);
    outb(0x3D4U, 14U);
    outb(0x3D5U, static_cast<uint8_t>((position >> 8U) & 0xFFU));
    outb(0x3D4U, 15U);
    outb(0x3D5U, static_cast<uint8_t>(position & 0xFFU));
}

void clear_row(uint16_t row) noexcept {
    for (uint16_t column = 0U; column < kVgaWidth; ++column) {
        g_vga[row * kVgaWidth + column] = static_cast<uint16_t>((kVgaColor << 8U) | ' ');
    }
}

void scroll_if_needed() noexcept {
    if (g_cursor_y < kVgaHeight) {
        return;
    }

    for (uint16_t row = 1U; row < kVgaHeight; ++row) {
        for (uint16_t column = 0U; column < kVgaWidth; ++column) {
            g_vga[(row - 1U) * kVgaWidth + column] = g_vga[row * kVgaWidth + column];
        }
    }

    g_cursor_y = static_cast<uint16_t>(kVgaHeight - 1U);
    clear_row(g_cursor_y);
}

void vga_write(char c) noexcept {
    if (c == '\n') {
        g_cursor_x = 0U;
        ++g_cursor_y;
        scroll_if_needed();
        update_cursor();
        return;
    }

    if (c == '\r') {
        g_cursor_x = 0U;
        update_cursor();
        return;
    }

    g_vga[g_cursor_y * kVgaWidth + g_cursor_x] = static_cast<uint16_t>((kVgaColor << 8U) | static_cast<uint8_t>(c));
    ++g_cursor_x;
    if (g_cursor_x >= kVgaWidth) {
        g_cursor_x = 0U;
        ++g_cursor_y;
    }
    scroll_if_needed();
    update_cursor();
}

void write_nibble(uint8_t nibble) noexcept {
    if (nibble < 10U) {
        write_char(static_cast<char>('0' + nibble));
        return;
    }
    write_char(static_cast<char>('A' + (nibble - 10U)));
}

void write_hex32_digits(uint32_t value) noexcept {
    for (int shift = 28; shift >= 0; shift -= 4) {
        write_nibble(static_cast<uint8_t>((value >> shift) & 0xFU));
    }
}

} // namespace

void initialize() noexcept {
    serial_initialize(kCom1);
    serial_initialize(kCom2);
    for (uint16_t row = 0U; row < kVgaHeight; ++row) {
        clear_row(row);
    }
    g_cursor_x = 0U;
    g_cursor_y = 0U;
    update_cursor();
}

void write_char(char c) noexcept {
    if (c == '\n') {
        serial_write(kCom1, '\r');
        serial_write(kCom1, '\n');
        vga_write('\n');
        return;
    }

    serial_write(kCom1, c);
    vga_write(c);
}

void write_string(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }

    while (*text != '\0') {
        write_char(*text);
        ++text;
    }
}

void debug_write_char(char c) noexcept {
    if (c == '\n') {
        serial_write(kCom2, '\r');
        serial_write(kCom2, '\n');
        return;
    }

    serial_write(kCom2, c);
}

void debug_write_string(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }

    while (*text != '\0') {
        debug_write_char(*text);
        ++text;
    }
}

char debug_read_char() noexcept {
    return serial_read(kCom2);
}

void write_bool(bool value) noexcept {
    write_string(value ? "yes" : "no");
}

void write_dec32(uint32_t value) noexcept {
    if (value == 0U) {
        write_char('0');
        return;
    }

    char buffer[10];
    uint32_t remaining = value;
    int index = 0;
    while (remaining != 0U) {
        buffer[index] = static_cast<char>('0' + (remaining % 10U));
        remaining /= 10U;
        ++index;
    }

    while (index > 0) {
        --index;
        write_char(buffer[index]);
    }
}

void write_hex32(uint32_t value) noexcept {
    write_string("0x");
    write_hex32_digits(value);
}

void write_hex64(uint64_t value) noexcept {
    write_string("0x");
    write_hex32_digits(static_cast<uint32_t>(value >> 32U));
    write_hex32_digits(static_cast<uint32_t>(value & 0xFFFFFFFFU));
}

void newline() noexcept {
    write_char('\n');
}

} // namespace xinim::i486::console
