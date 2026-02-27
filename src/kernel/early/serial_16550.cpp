#include "serial_16550.hpp"
#include <cstring>
#include <cstddef>

// Pull in proc table definition and constants for ps/mem commands.
// These are only used in the shell() method which runs in kernel context.
#include "../proc.hpp"

extern "C" {
    void reboot() noexcept;
    void halt() noexcept;
    int strcmp(const char* s1, const char* s2);
    std::size_t kernel_heap_used();
    std::size_t kernel_heap_total();
}

namespace {

// Format an unsigned integer into a decimal string (for kshell output).
// Returns pointer into a static buffer -- not reentrant, but fine for
// single-threaded kshell context.
const char* fmt_uint(std::size_t val) {
    static char buf[24];
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';
    if (val == 0) {
        *--p = '0';
        return p;
    }
    while (val > 0) {
        *--p = static_cast<char>('0' + val % 10);
        val /= 10;
    }
    return p;
}

const char* fmt_int(int val) {
    static char buf[24];
    if (val < 0) {
        buf[0] = '-';
        const char* s = fmt_uint(static_cast<std::size_t>(-val));
        std::size_t len = 0;
        while (s[len]) { buf[1 + len] = s[len]; ++len; }
        buf[1 + len] = '\0';
        return buf;
    }
    return fmt_uint(static_cast<std::size_t>(val));
}

} // anonymous namespace

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

char Serial16550::read_char() {
    while ((inb(base_ + 5) & 0x01) == 0) { /* wait for Data Ready */ }
    return static_cast<char>(inb(base_));
}

void Serial16550::shell() {
    char buf[128];
    int pos = 0;

    write("\nxinim kshell (type 'help' for commands)\n");
    write("xinim> ");

    while (true) {
        char c = read_char();
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            write("\n");
            if (::strcmp(buf, "help") == 0) {
                write("Commands: help, info, ps, mem, panic, reboot, halt\n");
            } else if (::strcmp(buf, "info") == 0) {
                write("XINIM Kernel v1.0.0 (x86_64, C++23)\n");
            } else if (::strcmp(buf, "ps") == 0) {
                // List active process slots from the MINIX-heritage proc table.
                write("PID  PRI  FLAGS  STATE\n");
                write("---  ---  -----  -----\n");
                int total = NR_TASKS + NR_PROCS;
                int active = 0;
                for (int i = 0; i < total; ++i) {
                    auto flags = static_cast<unsigned int>(proc[i].p_flags);
                    if (flags & P_SLOT_FREE)
                        continue;
                    ++active;
                    write(fmt_int(proc[i].p_pid));
                    write("    ");
                    write(fmt_int(proc[i].p_priority));
                    write("    ");
                    write(fmt_uint(static_cast<std::size_t>(flags)));
                    write("      ");
                    if (flags == 0)
                        write("READY");
                    else if (flags & SENDING)
                        write("SEND");
                    else if (flags & RECEIVING)
                        write("RECV");
                    else
                        write("OTHER");
                    write("\n");
                }
                write("Total active: ");
                write(fmt_int(active));
                write("/");
                write(fmt_int(total));
                write("\n");
            } else if (::strcmp(buf, "mem") == 0) {
                write("Kernel heap: ");
                write(fmt_uint(kernel_heap_used()));
                write(" / ");
                write(fmt_uint(kernel_heap_total()));
                write(" bytes (");
                if (kernel_heap_total() > 0) {
                    std::size_t pct = (kernel_heap_used() * 100) / kernel_heap_total();
                    write(fmt_uint(pct));
                } else {
                    write("0");
                }
                write("% used)\n");
            } else if (::strcmp(buf, "panic") == 0) {
                write("!!! KERNEL PANIC (manual trigger) !!!\n");
                while (true) {
#if defined(__x86_64__) || defined(__i386__)
                    __asm__ volatile("cli; hlt");
#endif
                }
            } else if (::strcmp(buf, "reboot") == 0) {
                write("Rebooting...\n");
                ::reboot();
            } else if (::strcmp(buf, "halt") == 0) {
                write("Halting...\n");
                ::halt();
            } else if (pos > 0) {
                write("Unknown command: ");
                write(buf);
                write("\n");
            }
            pos = 0;
            write("xinim> ");
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                write("\b \b");
            }
        } else if (pos < 127) {
            buf[pos++] = c;
            write_char(c);
        }
    }
}

void Serial16550::enable_rx_interrupt() {
    // Enable Received Data Available interrupt (IER bit 0).
    // Modem Control Register (MCR) bit 3 (OUT2) must be set for
    // the UART to assert the interrupt line on the PIC/APIC.
    std::uint8_t mcr = inb(base_ + 4);
    outb(base_ + 4, mcr | 0x08);  // Set OUT2
    outb(base_ + 1, 0x01);        // IER: enable RDA interrupt only
}

void Serial16550::isr_handler() {
    // Drain all available bytes from the UART FIFO into the ring buffer.
    while ((inb(base_ + 5) & 0x01) != 0) {
        char c = static_cast<char>(inb(base_));
        std::size_t next = (rx_head_ + 1) & (SERIAL_RX_BUF_SIZE - 1);
        if (next != rx_tail_) {  // Drop byte if buffer full
            rx_buf_[rx_head_] = c;
            rx_head_ = next;
        }
    }
}

bool Serial16550::try_read_char(char& c) {
    if (rx_head_ == rx_tail_) {
        return false;
    }
    c = rx_buf_[rx_tail_];
    rx_tail_ = (rx_tail_ + 1) & (SERIAL_RX_BUF_SIZE - 1);
    return true;
}

} // namespace xinim::early

xinim::early::Serial16550 early_serial(0x3f8);   // COM1: kernel log
xinim::early::Serial16550 kshell_serial(0x2f8);  // COM2: kshell
