#include "panic.hpp"
#include "early/serial_16550.hpp"

extern xinim::early::Serial16550 early_serial;

[[noreturn]] void kpanic(const char* msg) {
    early_serial.write("PANIC: ");
    early_serial.write(msg);
    early_serial.write("\n");
    for(;;) { __asm__ volatile ("cli; hlt"); }
}

