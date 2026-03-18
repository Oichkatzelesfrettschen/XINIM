#include "tty.hpp"

#include "console.hpp"

namespace xinim::i486::tty {
namespace {

constexpr uint32_t kCanonBufSize = 256U;
constexpr uint32_t kOutputBufSize = 256U;

// Canonical line buffer: accumulates characters until newline/EOF
char g_canon_buf[kCanonBufSize]{};
uint32_t g_canon_len = 0U;
bool g_canon_eof = false; // EOF signalled (Ctrl+D on empty line)

// Output ring buffer: completed lines (canonical) or raw chars (non-canonical)
char g_out_buf[kOutputBufSize]{};
uint32_t g_out_head = 0U;
uint32_t g_out_tail = 0U;
bool g_out_eof = false; // Next read should return 0

TermiosState g_termios{};
WindowSize g_winsize{25U, 80U, 0U, 0U};
bool g_lnext_pending = false; // VLNEXT: next char is literal (POSIX.1 c_cc extension)
uint32_t g_pending_ldisc_signal = 0U; // Signal from ISIG (2=SIGINT, 3=SIGQUIT, 20=SIGTSTP)

bool out_full() noexcept {
    return ((g_out_tail + 1U) % kOutputBufSize) == g_out_head;
}

void out_push(char c) noexcept {
    if (out_full()) {
        return;
    }
    g_out_buf[g_out_tail] = c;
    g_out_tail = (g_out_tail + 1U) % kOutputBufSize;
}

bool out_pop(char* c) noexcept {
    if (g_out_head == g_out_tail) {
        return false;
    }
    *c = g_out_buf[g_out_head];
    g_out_head = (g_out_head + 1U) % kOutputBufSize;
    return true;
}

// Output processing (c_oflag, per POSIX.1)
// OPOST: enable output processing. ONLCR: map NL to CR-NL.
void output_char(char c) noexcept {
    if ((g_termios.c_oflag & kOpost) != 0U) {
        if (c == '\n' && (g_termios.c_oflag & kOnlcr) != 0U) {
            console::tty_write_char('\r');
        }
    }
    console::tty_write_char(c);
}

void echo_char(char c) noexcept {
    if ((g_termios.c_lflag & kEcho) == 0U) {
        // ECHONL: echo NL even when ECHO is off (POSIX.1)
        if (c == '\n' && (g_termios.c_lflag & kEchonl) != 0U) {
            output_char('\n');
        }
        return;
    }
    if (c == '\n') {
        output_char('\n');
        return;
    }
    if (c == g_termios.c_cc[kVerase]) {
        if ((g_termios.c_lflag & kEchoe) != 0U) {
            console::tty_write_char('\b');
            console::tty_write_char(' ');
            console::tty_write_char('\b');
        }
        return;
    }
    if (c == g_termios.c_cc[kVkill]) {
        if ((g_termios.c_lflag & kEchok) != 0U) {
            output_char('\n');
        }
        return;
    }
    // Normal character echo
    if (static_cast<uint8_t>(c) < 0x20U && c != '\t') {
        console::tty_write_char('^');
        console::tty_write_char(static_cast<char>(c + '@'));
    } else {
        console::tty_write_char(c);
    }
}

// Flush the canonical buffer to the output ring
void flush_canon() noexcept {
    for (uint32_t i = 0U; i < g_canon_len; ++i) {
        out_push(g_canon_buf[i]);
    }
    g_canon_len = 0U;
}

// Input translation (c_iflag processing)
char translate_input(char c) noexcept {
    if (c == '\r') {
        if ((g_termios.c_iflag & kIgncr) != 0U) {
            return '\0'; // Discard CR
        }
        if ((g_termios.c_iflag & kIcrnl) != 0U) {
            return '\n'; // CR -> NL
        }
    }
    if (c == '\n' && (g_termios.c_iflag & kInlcr) != 0U) {
        return '\r'; // NL -> CR
    }
    return c;
}

} // namespace

void initialize() noexcept {
    g_termios = {};
    g_termios.c_iflag = kIcrnl; // CR -> NL
    g_termios.c_oflag = 0000005U; // OPOST | ONLCR
    g_termios.c_cflag = 0000277U; // B38400 | CS8 | CREAD | HUPCL
    g_termios.c_lflag = kIcanon | kEcho | kEchoe | kEchok | kIsig;
    g_termios.c_cc[kVintr] = 3U;    // Ctrl+C
    g_termios.c_cc[kVquit] = 28U;   // Ctrl+backslash
    g_termios.c_cc[kVerase] = 127U;  // DEL
    g_termios.c_cc[kVkill] = 21U;   // Ctrl+U
    g_termios.c_cc[kVeof] = 4U;     // Ctrl+D
    g_termios.c_cc[kVtime] = 0U;
    g_termios.c_cc[kVmin] = 1U;
    g_termios.c_cc[kVstart] = 17U;  // Ctrl+Q
    g_termios.c_cc[kVstop] = 19U;   // Ctrl+S
    g_termios.c_cc[kVsusp] = 26U;   // Ctrl+Z
    g_termios.c_cc[kVwerase] = 23U;  // Ctrl+W
    g_termios.c_cc[kVreprint] = 18U; // Ctrl+R
    g_termios.c_cc[kVlnext] = 22U;  // Ctrl+V (literal next, POSIX.1)

    g_canon_len = 0U;
    g_canon_eof = false;
    g_out_head = 0U;
    g_out_tail = 0U;
    g_out_eof = false;
    g_lnext_pending = false;
    g_pending_ldisc_signal = 0U;
    g_winsize = {25U, 80U, 0U, 0U};
}

void input_char(char c) noexcept {
    c = translate_input(c);
    if (c == '\0') {
        return; // Discarded by input translation
    }

    // VLNEXT (Ctrl+V): literal next character (POSIX.1 c_cc extension).
    // When IEXTEN is set and VLNEXT is received, the next character is
    // treated literally -- no line editing, no signal generation.
    if (g_lnext_pending) {
        g_lnext_pending = false;
        // Insert literally into canonical buffer or output
        const bool canonical = (g_termios.c_lflag & kIcanon) != 0U;
        if (canonical) {
            if (g_canon_len < kCanonBufSize - 1U) {
                g_canon_buf[g_canon_len] = c;
                ++g_canon_len;
                echo_char(c);
            }
        } else {
            echo_char(c);
            out_push(c);
        }
        return;
    }

    // Check for VLNEXT trigger
    if ((g_termios.c_lflag & kIexten) != 0U &&
        g_termios.c_cc[kVlnext] != 0U &&
        static_cast<uint8_t>(c) == g_termios.c_cc[kVlnext]) {
        g_lnext_pending = true;
        // Echo ^V indicator
        if ((g_termios.c_lflag & kEcho) != 0U) {
            console::tty_write_char('^');
            console::tty_write_char('\b');
        }
        return;
    }

    // ISIG: generate signals for VINTR, VQUIT, VSUSP
    if ((g_termios.c_lflag & kIsig) != 0U) {
        const uint8_t uc_sig = static_cast<uint8_t>(c);
        if (g_termios.c_cc[kVintr] != 0U && uc_sig == g_termios.c_cc[kVintr]) {
            g_pending_ldisc_signal = 2U; // SIGINT
            if ((g_termios.c_lflag & kNoflsh) == 0U) {
                g_canon_len = 0U;
                g_out_head = 0U;
                g_out_tail = 0U;
            }
            return;
        }
        if (g_termios.c_cc[kVquit] != 0U && uc_sig == g_termios.c_cc[kVquit]) {
            g_pending_ldisc_signal = 3U; // SIGQUIT
            if ((g_termios.c_lflag & kNoflsh) == 0U) {
                g_canon_len = 0U;
                g_out_head = 0U;
                g_out_tail = 0U;
            }
            return;
        }
        if (g_termios.c_cc[kVsusp] != 0U && uc_sig == g_termios.c_cc[kVsusp]) {
            g_pending_ldisc_signal = 20U; // SIGTSTP
            return;
        }
    }

    const bool canonical = (g_termios.c_lflag & kIcanon) != 0U;

    if (!canonical) {
        // Raw/non-canonical mode: pass through immediately
        echo_char(c);
        out_push(c);
        return;
    }

    // Canonical mode: line editing
    const uint8_t uc = static_cast<uint8_t>(c);

    // VERASE: delete last character
    if (uc == g_termios.c_cc[kVerase]) {
        if (g_canon_len > 0U) {
            --g_canon_len;
            echo_char(c);
        }
        return;
    }

    // VKILL: delete entire line
    if (uc == g_termios.c_cc[kVkill]) {
        // Erase each character visually
        if ((g_termios.c_lflag & kEcho) != 0U && (g_termios.c_lflag & kEchoe) != 0U) {
            while (g_canon_len > 0U) {
                --g_canon_len;
                console::tty_write_char('\b');
                console::tty_write_char(' ');
                console::tty_write_char('\b');
            }
        } else {
            g_canon_len = 0U;
            echo_char(c);
        }
        return;
    }

    // VWERASE: word erase (Ctrl+W) -- erase backwards to previous whitespace
    if (g_termios.c_cc[kVwerase] != 0U && uc == g_termios.c_cc[kVwerase]) {
        // Skip trailing whitespace
        while (g_canon_len > 0U && (g_canon_buf[g_canon_len - 1U] == ' ' ||
                                     g_canon_buf[g_canon_len - 1U] == '\t')) {
            --g_canon_len;
            if ((g_termios.c_lflag & kEcho) != 0U && (g_termios.c_lflag & kEchoe) != 0U) {
                console::tty_write_char('\b');
                console::tty_write_char(' ');
                console::tty_write_char('\b');
            }
        }
        // Erase the word
        while (g_canon_len > 0U && g_canon_buf[g_canon_len - 1U] != ' ' &&
               g_canon_buf[g_canon_len - 1U] != '\t') {
            --g_canon_len;
            if ((g_termios.c_lflag & kEcho) != 0U && (g_termios.c_lflag & kEchoe) != 0U) {
                console::tty_write_char('\b');
                console::tty_write_char(' ');
                console::tty_write_char('\b');
            }
        }
        return;
    }

    // VREPRINT: reprint canonical buffer (Ctrl+R)
    if (g_termios.c_cc[kVreprint] != 0U && uc == g_termios.c_cc[kVreprint]) {
        console::tty_write_char('^');
        console::tty_write_char('R');
        output_char('\n');
        for (uint32_t i = 0U; i < g_canon_len; ++i) {
            const char ch = g_canon_buf[i];
            if (static_cast<uint8_t>(ch) < 0x20U && ch != '\t') {
                console::tty_write_char('^');
                console::tty_write_char(static_cast<char>(ch + '@'));
            } else {
                console::tty_write_char(ch);
            }
        }
        return;
    }

    // VEOF: end of file
    if (uc == g_termios.c_cc[kVeof]) {
        if (g_canon_len == 0U) {
            // EOF on empty line: signal EOF to reader
            g_out_eof = true;
        } else {
            // EOF with data: flush buffer without newline
            flush_canon();
        }
        return;
    }

    // Newline: complete the line
    if (c == '\n') {
        g_canon_buf[g_canon_len] = '\n';
        if (g_canon_len + 1U < kCanonBufSize) {
            ++g_canon_len;
        }
        echo_char('\n');
        flush_canon();
        return;
    }

    // Regular character: append to canonical buffer
    if (g_canon_len < kCanonBufSize - 1U) {
        g_canon_buf[g_canon_len] = c;
        ++g_canon_len;
        echo_char(c);
    }
    // Buffer full: silently drop
}

int read(void* buffer, uint32_t count) noexcept {
    if (buffer == nullptr || count == 0U) {
        return 0;
    }

    // Check for EOF
    if (g_out_eof) {
        g_out_eof = false;
        return 0; // EOF
    }

    auto* dst = static_cast<uint8_t*>(buffer);
    uint32_t read_count = 0U;
    const bool canonical = (g_termios.c_lflag & kIcanon) != 0U;

    if (canonical) {
        // In canonical mode, return up to one line
        while (read_count < count) {
            char c = '\0';
            if (!out_pop(&c)) {
                break;
            }
            dst[read_count] = static_cast<uint8_t>(c);
            ++read_count;
            if (c == '\n') {
                break; // End of line
            }
        }
    } else {
        // Non-canonical mode: return what's available up to count
        while (read_count < count) {
            char c = '\0';
            if (!out_pop(&c)) {
                break;
            }
            dst[read_count] = static_cast<uint8_t>(c);
            ++read_count;
        }
    }

    if (read_count == 0U) {
        return -1; // Would block
    }
    return static_cast<int>(read_count);
}

bool has_input() noexcept {
    if (g_out_eof) {
        return true; // EOF is "data available"
    }
    return g_out_head != g_out_tail;
}

bool canonical_line_ready() noexcept {
    if ((g_termios.c_lflag & kIcanon) == 0U) {
        return g_out_head != g_out_tail; // Non-canonical: any data is ready
    }
    if (g_out_eof) {
        return true;
    }
    // Scan the output buffer for a newline
    uint32_t pos = g_out_head;
    while (pos != g_out_tail) {
        if (g_out_buf[pos] == '\n') {
            return true;
        }
        pos = (pos + 1U) % kOutputBufSize;
    }
    return false;
}

void get_termios(TermiosState* out) noexcept {
    if (out != nullptr) {
        *out = g_termios;
    }
}

void set_termios(const TermiosState* state) noexcept {
    if (state != nullptr) {
        g_termios = *state;
    }
}

void get_winsize(WindowSize* out) noexcept {
    if (out != nullptr) {
        *out = g_winsize;
    }
}

void set_winsize(const WindowSize* ws) noexcept {
    if (ws != nullptr) {
        g_winsize = *ws;
    }
}

uint32_t consume_pending_ldisc_signal() noexcept {
    const uint32_t sig = g_pending_ldisc_signal;
    g_pending_ldisc_signal = 0U;
    return sig;
}

} // namespace xinim::i486::tty
