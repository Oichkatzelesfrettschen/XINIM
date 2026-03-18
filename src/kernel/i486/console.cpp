#include "console.hpp"

#include "tty.hpp"

namespace xinim::i486::console {
namespace {

constexpr uint16_t kCom1 = 0x3F8U;
constexpr uint16_t kCom2 = 0x2F8U;
constexpr uint16_t kKeyboardDataPort = 0x60U;
constexpr uint16_t kKeyboardStatusPort = 0x64U;
constexpr uint16_t kVgaWidth = 80U;
constexpr uint16_t kVgaHeight = 25U;
constexpr uint8_t kVgaDefaultColor = 0x0FU;
volatile uint16_t* const g_vga = reinterpret_cast<volatile uint16_t*>(0xB8000U);
uint16_t g_cursor_x = 0U;
uint16_t g_cursor_y = 0U;
uint8_t g_vga_color = kVgaDefaultColor;

// ANSI escape sequence parser state machine
enum class AnsiState : uint8_t {
    Normal = 0,
    GotEsc = 1,     // Received ESC (0x1B)
    InCSI = 2,      // Received ESC[, collecting parameters
};
AnsiState g_ansi_state = AnsiState::Normal;
uint32_t g_ansi_params[8]{};
uint32_t g_ansi_param_count = 0U;
uint32_t g_ansi_current_param = 0U;
bool g_ansi_has_digit = false;
bool g_keyboard_shift = false;
bool g_keyboard_ctrl = false;
bool g_keyboard_caps_lock = false;
bool g_keyboard_extended = false;
volatile uint32_t g_pending_tty_signal = 0U; // 2=SIGINT(Ctrl+C), 20=SIGTSTP(Ctrl+Z)

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

void drain_tty_devices() noexcept {
    while (serial_has_data(kCom2)) {
        const char value = serial_read(kCom2);
        if (!is_valid_tty_serial_char(value)) {
            continue;
        }
        xinim::i486::tty::input_char(value);
    }
    while (keyboard_has_data()) {
        char output = '\0';
        const uint8_t scancode = inb(kKeyboardDataPort);
        if (translate_keyboard_scancode(scancode, output)) {
            xinim::i486::tty::input_char(output);
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
        g_vga[row * kVgaWidth + column] = static_cast<uint16_t>((g_vga_color << 8U) | ' ');
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

// Map ANSI color code (30-37) to VGA color attribute
uint8_t ansi_to_vga_fg(uint32_t code) noexcept {
    // ANSI: 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
    // VGA:  0=black 4=red 2=green 6=brown  1=blue 5=magenta  3=cyan  7=lgray
    static constexpr uint8_t kMap[8] = {0, 4, 2, 6, 1, 5, 3, 7};
    if (code >= 30U && code <= 37U) return kMap[code - 30U];
    return 7U;
}

uint8_t ansi_to_vga_bg(uint32_t code) noexcept {
    static constexpr uint8_t kMap[8] = {0, 4, 2, 6, 1, 5, 3, 7};
    if (code >= 40U && code <= 47U) return kMap[code - 40U];
    return 0U;
}

void ansi_push_param() noexcept {
    if (g_ansi_param_count < 8U) {
        g_ansi_params[g_ansi_param_count++] = g_ansi_current_param;
    }
    g_ansi_current_param = 0U;
    g_ansi_has_digit = false;
}

uint32_t ansi_param(uint32_t index, uint32_t fallback) noexcept {
    return (index < g_ansi_param_count) ? g_ansi_params[index] : fallback;
}

void execute_csi(char final_char) noexcept {
    // Push last parameter if digits were seen
    if (g_ansi_has_digit || g_ansi_param_count == 0U) {
        ansi_push_param();
    }

    switch (final_char) {
    case 'A': { // Cursor up
        uint32_t n = ansi_param(0U, 1U);
        g_cursor_y = (g_cursor_y >= n) ? static_cast<uint16_t>(g_cursor_y - n) : 0U;
        break;
    }
    case 'B': { // Cursor down
        uint32_t n = ansi_param(0U, 1U);
        uint16_t ny = static_cast<uint16_t>(g_cursor_y + n);
        g_cursor_y = (ny < kVgaHeight) ? ny : static_cast<uint16_t>(kVgaHeight - 1U);
        break;
    }
    case 'C': { // Cursor forward
        uint32_t n = ansi_param(0U, 1U);
        uint16_t nx = static_cast<uint16_t>(g_cursor_x + n);
        g_cursor_x = (nx < kVgaWidth) ? nx : static_cast<uint16_t>(kVgaWidth - 1U);
        break;
    }
    case 'D': { // Cursor backward
        uint32_t n = ansi_param(0U, 1U);
        g_cursor_x = (g_cursor_x >= n) ? static_cast<uint16_t>(g_cursor_x - n) : 0U;
        break;
    }
    case 'H':
    case 'f': { // Cursor position (row;col, 1-based)
        uint32_t row = ansi_param(0U, 1U);
        uint32_t col = (g_ansi_param_count >= 2U) ? g_ansi_params[1] : 1U;
        if (row > 0U) --row;
        if (col > 0U) --col;
        g_cursor_y = (row < kVgaHeight) ? static_cast<uint16_t>(row) : static_cast<uint16_t>(kVgaHeight - 1U);
        g_cursor_x = (col < kVgaWidth) ? static_cast<uint16_t>(col) : static_cast<uint16_t>(kVgaWidth - 1U);
        break;
    }
    case 'J': { // Erase in display
        uint32_t mode = ansi_param(0U, 0U);
        if (mode == 2U) {
            // Clear entire screen
            for (uint16_t r = 0U; r < kVgaHeight; ++r) clear_row(r);
            g_cursor_x = 0U;
            g_cursor_y = 0U;
        } else if (mode == 0U) {
            // Clear from cursor to end
            for (uint16_t col = g_cursor_x; col < kVgaWidth; ++col) {
                g_vga[g_cursor_y * kVgaWidth + col] = static_cast<uint16_t>((g_vga_color << 8U) | ' ');
            }
            for (uint16_t r = static_cast<uint16_t>(g_cursor_y + 1U); r < kVgaHeight; ++r) clear_row(r);
        }
        break;
    }
    case 'K': { // Erase in line
        uint32_t mode = ansi_param(0U, 0U);
        if (mode == 0U) {
            // Clear from cursor to end of line
            for (uint16_t col = g_cursor_x; col < kVgaWidth; ++col) {
                g_vga[g_cursor_y * kVgaWidth + col] = static_cast<uint16_t>((g_vga_color << 8U) | ' ');
            }
        } else if (mode == 2U) {
            // Clear entire line
            clear_row(g_cursor_y);
        }
        break;
    }
    case 'm': { // SGR (Select Graphic Rendition)
        for (uint32_t i = 0U; i < g_ansi_param_count; ++i) {
            uint32_t p = g_ansi_params[i];
            if (p == 0U) {
                g_vga_color = kVgaDefaultColor; // Reset
            } else if (p == 1U) {
                g_vga_color |= 0x08U; // Bold (bright foreground)
            } else if (p >= 30U && p <= 37U) {
                g_vga_color = static_cast<uint8_t>((g_vga_color & 0xF8U) | ansi_to_vga_fg(p));
            } else if (p >= 40U && p <= 47U) {
                g_vga_color = static_cast<uint8_t>((g_vga_color & 0x0FU) | (ansi_to_vga_bg(p) << 4U));
            } else if (p == 7U) {
                // Reverse video
                uint8_t fg = g_vga_color & 0x0FU;
                uint8_t bg = (g_vga_color >> 4U) & 0x0FU;
                g_vga_color = static_cast<uint8_t>((fg << 4U) | bg);
            }
        }
        if (g_ansi_param_count == 0U) {
            g_vga_color = kVgaDefaultColor; // ESC[m = reset
        }
        break;
    }
    default:
        break; // Unknown CSI sequence, ignore
    }
    update_cursor();
}

void vga_write(char c) noexcept {
    switch (g_ansi_state) {
    case AnsiState::Normal:
        if (c == '\x1B') {
            g_ansi_state = AnsiState::GotEsc;
            return;
        }
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
        if (c == '\b' || c == 127) {
            if (g_cursor_x > 0U) {
                --g_cursor_x;
                g_vga[g_cursor_y * kVgaWidth + g_cursor_x] =
                    static_cast<uint16_t>((g_vga_color << 8U) | ' ');
                update_cursor();
            }
            return;
        }
        if (c == '\t') {
            uint16_t next = static_cast<uint16_t>((g_cursor_x + 8U) & ~7U);
            g_cursor_x = (next < kVgaWidth) ? next : static_cast<uint16_t>(kVgaWidth - 1U);
            update_cursor();
            return;
        }
        g_vga[g_cursor_y * kVgaWidth + g_cursor_x] =
            static_cast<uint16_t>((g_vga_color << 8U) | static_cast<uint8_t>(c));
        ++g_cursor_x;
        if (g_cursor_x >= kVgaWidth) {
            g_cursor_x = 0U;
            ++g_cursor_y;
        }
        scroll_if_needed();
        update_cursor();
        return;

    case AnsiState::GotEsc:
        if (c == '[') {
            g_ansi_state = AnsiState::InCSI;
            g_ansi_param_count = 0U;
            g_ansi_current_param = 0U;
            g_ansi_has_digit = false;
            return;
        }
        // Not a CSI sequence, emit the ESC and the char
        g_ansi_state = AnsiState::Normal;
        return;

    case AnsiState::InCSI:
        if (c >= '0' && c <= '9') {
            g_ansi_current_param = g_ansi_current_param * 10U + static_cast<uint32_t>(c - '0');
            g_ansi_has_digit = true;
            return;
        }
        if (c == ';') {
            ansi_push_param();
            return;
        }
        if (c >= 0x40 && c <= 0x7E) {
            // Final byte -- execute the CSI command
            execute_csi(c);
            g_ansi_state = AnsiState::Normal;
            return;
        }
        // Unknown intermediate byte, reset
        g_ansi_state = AnsiState::Normal;
        return;
    }
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

void force_vga_text_mode() noexcept {
    // Reset Attribute Controller flip-flop by reading Input Status Register 1
    inb(0x3DAU);
    // Disable display to reprogram
    outb(0x3C0U, 0x00U);

    // Miscellaneous Output Register: enable 80-col text, color mode
    outb(0x3C2U, 0x67U);

    // Sequencer: reset, then configure for text mode
    outb(0x3C4U, 0x00U); outb(0x3C5U, 0x03U); // Reset: async+sync
    outb(0x3C4U, 0x01U); outb(0x3C5U, 0x00U); // Clocking: 8-dot chars
    outb(0x3C4U, 0x02U); outb(0x3C5U, 0x03U); // Map mask: planes 0+1
    outb(0x3C4U, 0x03U); outb(0x3C5U, 0x00U); // Char map select: 0
    outb(0x3C4U, 0x04U); outb(0x3C5U, 0x02U); // Memory mode: O/E, !chain4

    // Unlock CRTC registers
    outb(0x3D4U, 0x11U); outb(0x3D5U, inb(0x3D5U) & 0x7FU);

    // CRTC registers for 80x25 text mode (720x400 @ 70 Hz timings)
    static constexpr uint8_t crtc_regs[] = {
        0x5FU, 0x4FU, 0x50U, 0x82U, 0x55U, 0x81U, 0xBFU, 0x1FU,
        0x00U, 0x4FU, 0x0DU, 0x0EU, 0x00U, 0x00U, 0x00U, 0x00U,
        0x9CU, 0x8EU, 0x8FU, 0x28U, 0x1FU, 0x96U, 0xB9U, 0xA3U,
        0xFFU,
    };
    for (uint8_t i = 0U; i < sizeof(crtc_regs); ++i) {
        outb(0x3D4U, i);
        outb(0x3D5U, crtc_regs[i]);
    }

    // Graphics Controller: text mode defaults
    outb(0x3CEU, 0x00U); outb(0x3CFU, 0x00U); // Set/Reset
    outb(0x3CEU, 0x01U); outb(0x3CFU, 0x00U); // Enable Set/Reset
    outb(0x3CEU, 0x02U); outb(0x3CFU, 0x00U); // Color Compare
    outb(0x3CEU, 0x03U); outb(0x3CFU, 0x00U); // Data Rotate
    outb(0x3CEU, 0x04U); outb(0x3CFU, 0x00U); // Read Map Select
    outb(0x3CEU, 0x05U); outb(0x3CFU, 0x10U); // Mode: O/E text
    outb(0x3CEU, 0x06U); outb(0x3CFU, 0x0EU); // Misc: text, B8000
    outb(0x3CEU, 0x07U); outb(0x3CFU, 0x00U); // Color Don't Care
    outb(0x3CEU, 0x08U); outb(0x3CFU, 0xFFU); // Bit Mask

    // Attribute Controller: standard text mode palette
    inb(0x3DAU); // Reset flip-flop
    static constexpr uint8_t attr_regs[] = {
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x14U, 0x07U,
        0x38U, 0x39U, 0x3AU, 0x3BU, 0x3CU, 0x3DU, 0x3EU, 0x3FU,
        0x0CU, 0x00U, 0x0FU, 0x08U, 0x00U,
    };
    for (uint8_t i = 0U; i < sizeof(attr_regs); ++i) {
        inb(0x3DAU);
        outb(0x3C0U, i);
        outb(0x3C0U, attr_regs[i]);
    }

    // Re-enable display
    inb(0x3DAU);
    outb(0x3C0U, 0x20U);
}

void initialize() noexcept {
    serial_initialize(kCom1);
    serial_initialize(kCom2);
    force_vga_text_mode();
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
    return xinim::i486::tty::has_input();
}

bool tty_try_read_char(char* out) noexcept {
    drain_tty_devices();
    if (out == nullptr) {
        return false;
    }
    char buf = '\0';
    const int result = xinim::i486::tty::read(&buf, 1U);
    if (result <= 0) {
        return false;
    }
    *out = buf;
    return true;
}

void vga_write_char(char c) noexcept {
    vga_write(c);
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
