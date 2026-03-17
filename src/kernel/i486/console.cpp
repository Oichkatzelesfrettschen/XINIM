#include "console.hpp"

namespace xinim::i486::console {
namespace {

constexpr uint16_t kCom1 = 0x3F8U;
constexpr uint16_t kCom2 = 0x2F8U;
constexpr uint16_t kKeyboardDataPort = 0x60U;
constexpr uint16_t kKeyboardStatusPort = 0x64U;
constexpr uint16_t kVgaWidth = 80U;
constexpr uint16_t kVgaHeight = 25U;
constexpr uint8_t kVgaColor = 0x0FU;
constexpr uint32_t kTtyRxBufferSize = 256U;

volatile uint16_t* const g_vga = reinterpret_cast<volatile uint16_t*>(0xB8000U);
uint16_t g_cursor_x = 0U;
uint16_t g_cursor_y = 0U;
bool g_keyboard_shift = false;
bool g_keyboard_ctrl = false;
bool g_keyboard_caps_lock = false;
bool g_keyboard_extended = false;
volatile uint32_t g_pending_tty_signal = 0U; // 2=SIGINT(Ctrl+C), 20=SIGTSTP(Ctrl+Z)
char g_tty_rx_buffer[kTtyRxBufferSize]{};
uint32_t g_tty_rx_head = 0U;
uint32_t g_tty_rx_tail = 0U;

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

[[nodiscard]] bool serial_has_data(uint16_t port) noexcept {
    return (inb(port + 5U) & 0x01U) != 0U;
}

[[nodiscard]] bool is_valid_tty_serial_char(char value) noexcept {
    if (value == '\n' || value == '\r' || value == '\t' || value == 27 || value == 127) {
        return true;
    }
    const uint8_t raw = static_cast<uint8_t>(value);
    return raw >= 0x20U && raw <= 0x7EU;
}

[[nodiscard]] bool keyboard_has_data() noexcept;
[[nodiscard]] bool translate_keyboard_scancode(uint8_t scancode, char& output) noexcept;

[[nodiscard]] bool tty_rx_buffer_empty() noexcept {
    return g_tty_rx_head == g_tty_rx_tail;
}

[[nodiscard]] bool tty_rx_buffer_full() noexcept {
    return ((g_tty_rx_tail + 1U) % kTtyRxBufferSize) == g_tty_rx_head;
}

void tty_rx_push(char value) noexcept {
    if (tty_rx_buffer_full()) {
        return;
    }
    g_tty_rx_buffer[g_tty_rx_tail] = value;
    g_tty_rx_tail = (g_tty_rx_tail + 1U) % kTtyRxBufferSize;
}

[[nodiscard]] bool tty_rx_pop(char* out) noexcept {
    if (out == nullptr || tty_rx_buffer_empty()) {
        return false;
    }
    *out = g_tty_rx_buffer[g_tty_rx_head];
    g_tty_rx_head = (g_tty_rx_head + 1U) % kTtyRxBufferSize;
    return true;
}

void drain_tty_devices() noexcept {
    while (serial_has_data(kCom2)) {
        const char value = serial_read(kCom2);
        if (!is_valid_tty_serial_char(value)) {
            continue;
        }
        tty_rx_push(value);
    }
    while (keyboard_has_data()) {
        char output = '\0';
        const uint8_t scancode = inb(kKeyboardDataPort);
        if (translate_keyboard_scancode(scancode, output)) {
            tty_rx_push(output);
        }
    }
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

[[nodiscard]] bool keyboard_has_data() noexcept {
    return (inb(kKeyboardStatusPort) & 0x01U) != 0U;
}

[[nodiscard]] char translate_letter(char base) noexcept {
    const bool uppercase = g_keyboard_shift != g_keyboard_caps_lock;
    if (uppercase) {
        return static_cast<char>(base - ('a' - 'A'));
    }
    return base;
}

[[nodiscard]] bool translate_keyboard_scancode(uint8_t scancode, char& output) noexcept {
    if (scancode == 0xE0U) {
        g_keyboard_extended = true;
        return false;
    }

    if ((scancode & 0x80U) != 0U) {
        const uint8_t released = static_cast<uint8_t>(scancode & 0x7FU);
        if (released == 0x2AU || released == 0x36U) {
            g_keyboard_shift = false;
        }
        if (released == 0x1DU) {
            g_keyboard_ctrl = false;
        }
        if (g_keyboard_extended) {
            g_keyboard_extended = false;
        }
        return false;
    }

    if (g_keyboard_extended) {
        g_keyboard_extended = false;
        return false;
    }

    switch (scancode) {
    case 0x1DU:
        g_keyboard_ctrl = true;
        return false;
    case 0x2AU:
    case 0x36U:
        g_keyboard_shift = true;
        return false;
    case 0x3AU:
        g_keyboard_caps_lock = !g_keyboard_caps_lock;
        return false;
    case 0x01U:
        output = 27;
        return true;
    case 0x0EU:
        output = 127;
        return true;
    case 0x0FU:
        output = '\t';
        return true;
    case 0x2EU: // 'c' key
        if (g_keyboard_ctrl) {
            g_pending_tty_signal = 2U; // SIGINT
            output = 3; // ETX
            return true;
        }
        output = translate_letter('c');
        return true;
    case 0x2CU: // 'z' key
        if (g_keyboard_ctrl) {
            g_pending_tty_signal = 20U; // SIGTSTP
            output = 26; // SUB
            return true;
        }
        output = translate_letter('z');
        return true;
    case 0x1CU:
        output = '\n';
        return true;
    case 0x39U:
        output = ' ';
        return true;
    case 0x02U:
        output = g_keyboard_shift ? '!' : '1';
        return true;
    case 0x03U:
        output = g_keyboard_shift ? '@' : '2';
        return true;
    case 0x04U:
        output = g_keyboard_shift ? '#' : '3';
        return true;
    case 0x05U:
        output = g_keyboard_shift ? '$' : '4';
        return true;
    case 0x06U:
        output = g_keyboard_shift ? '%' : '5';
        return true;
    case 0x07U:
        output = g_keyboard_shift ? '^' : '6';
        return true;
    case 0x08U:
        output = g_keyboard_shift ? '&' : '7';
        return true;
    case 0x09U:
        output = g_keyboard_shift ? '*' : '8';
        return true;
    case 0x0AU:
        output = g_keyboard_shift ? '(' : '9';
        return true;
    case 0x0BU:
        output = g_keyboard_shift ? ')' : '0';
        return true;
    case 0x0CU:
        output = g_keyboard_shift ? '_' : '-';
        return true;
    case 0x0DU:
        output = g_keyboard_shift ? '+' : '=';
        return true;
    case 0x10U:
        output = translate_letter('q');
        return true;
    case 0x11U:
        output = translate_letter('w');
        return true;
    case 0x12U:
        output = translate_letter('e');
        return true;
    case 0x13U:
        output = translate_letter('r');
        return true;
    case 0x14U:
        output = translate_letter('t');
        return true;
    case 0x15U:
        output = translate_letter('y');
        return true;
    case 0x16U:
        output = translate_letter('u');
        return true;
    case 0x17U:
        output = translate_letter('i');
        return true;
    case 0x18U:
        output = translate_letter('o');
        return true;
    case 0x19U:
        output = translate_letter('p');
        return true;
    case 0x1AU:
        output = g_keyboard_shift ? '{' : '[';
        return true;
    case 0x1BU:
        output = g_keyboard_shift ? '}' : ']';
        return true;
    case 0x1EU:
        output = translate_letter('a');
        return true;
    case 0x1FU:
        output = translate_letter('s');
        return true;
    case 0x20U:
        output = translate_letter('d');
        return true;
    case 0x21U:
        output = translate_letter('f');
        return true;
    case 0x22U:
        output = translate_letter('g');
        return true;
    case 0x23U:
        output = translate_letter('h');
        return true;
    case 0x24U:
        output = translate_letter('j');
        return true;
    case 0x25U:
        output = translate_letter('k');
        return true;
    case 0x26U:
        output = translate_letter('l');
        return true;
    case 0x27U:
        output = g_keyboard_shift ? ':' : ';';
        return true;
    case 0x28U:
        output = g_keyboard_shift ? '"' : '\'';
        return true;
    case 0x29U:
        output = g_keyboard_shift ? '~' : '`';
        return true;
    case 0x2BU:
        output = g_keyboard_shift ? '|' : '\\';
        return true;
    case 0x2DU:
        output = translate_letter('x');
        return true;
    case 0x2FU:
        output = translate_letter('v');
        return true;
    case 0x30U:
        output = translate_letter('b');
        return true;
    case 0x31U:
        output = translate_letter('n');
        return true;
    case 0x32U:
        output = translate_letter('m');
        return true;
    case 0x33U:
        output = g_keyboard_shift ? '<' : ',';
        return true;
    case 0x34U:
        output = g_keyboard_shift ? '>' : '.';
        return true;
    case 0x35U:
        output = g_keyboard_shift ? '?' : '/';
        return true;
    default:
        return false;
    }
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

void tty_write_char(char c) noexcept {
    drain_tty_devices();
    if (c == '\n') {
        serial_write(kCom2, '\r');
        serial_write(kCom2, '\n');
        vga_write('\n');
        return;
    }
    serial_write(kCom2, c);
    vga_write(c);
}

void tty_write_string(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        tty_write_char(*text);
        ++text;
    }
}

char tty_read_char() noexcept {
    for (;;) {
        char output = '\0';
        if (tty_try_read_char(&output)) {
            return output;
        }
    }
}

void tty_poll_input() noexcept {
    drain_tty_devices();
}

bool tty_has_input() noexcept {
    drain_tty_devices();
    return !tty_rx_buffer_empty();
}

bool tty_try_read_char(char* out) noexcept {
    drain_tty_devices();
    return tty_rx_pop(out);
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

uint32_t consume_pending_tty_signal() noexcept {
    const uint32_t sig = g_pending_tty_signal;
    g_pending_tty_signal = 0U;
    return sig;
}

} // namespace xinim::i486::console
