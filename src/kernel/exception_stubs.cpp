// Stub exception handlers for bare-metal kernel.
// These are called from assembly ISR vectors.

#include "sys/com.hpp"
#include "const.hpp"
#include "proc.hpp"
#include "glo.hpp"
#include "early/serial_16550.hpp"

extern xinim::early::Serial16550 early_serial;

extern "C" {

void surprise() noexcept {
    early_serial.write("Unexpected interrupt!\n");
}

void clock_int() noexcept {
    static message m;
    m.m_type = CLOCK_TICK;
    interrupt(CLOCK, &m);
}

void tty_int() noexcept {
    static message m;
    m.m_type = TTY_CHAR_INT;
    interrupt(TTY, &m);
}

void div_trap() noexcept {
    early_serial.write("Divide by zero trap!\n");
}

void trap() noexcept {
    early_serial.write("General trap!\n");
}

void build_sig(struct sig_info *dst, struct proc *rp, int sig) noexcept {
    (void)dst; (void)rp; (void)sig;
}

unsigned char get_byte(unsigned int seg, unsigned int off) noexcept {
    (void)seg; (void)off;
    return 0;
}

void tty_sig_init() noexcept {}

void isr_common_handler(int vector) noexcept {
    (void)vector;
}

} // extern "C"
