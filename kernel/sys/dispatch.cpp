#include <stdint.h>
#include <xinim/sys/syscalls.h>
#include "../early/serial_16550.hpp"
#include "../time/monotonic.hpp"

extern xinim::early::Serial16550 early_serial;

static uint64_t sys_debug_write_impl(const char* s, uint64_t n) {
    for (uint64_t i=0;i<n;i++) early_serial.write_char(s[i]);
    return n;
}

static uint64_t sys_monotonic_ns_impl() {
    return xinim::time::monotonic_ns();
}

extern "C" uint64_t xinim_syscall_dispatch(uint64_t no,
    uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    switch (no) {
        case SYS_debug_write: return sys_debug_write_impl((const char*)a0, a1);
        case SYS_monotonic_ns: return sys_monotonic_ns_impl();
        default: return (uint64_t)-1;
    }
}

