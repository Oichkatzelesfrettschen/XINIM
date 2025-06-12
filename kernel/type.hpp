#pragma once
// Modernized for C++17

/* The 'pc_psw' struct is machine dependent.  It must contain the information
 * pushed onto the stack by an interrupt, in the same format as the hardware
 * creates and expects.  It is used for storing the interrupt status after a
 * trap or interrupt, as well as for causing interrupts for signals.
 */

#include "../include/defs.hpp" // Changed to .hpp
#include <cstdint>             // For uint64_t (if u64_t isn't directly std::uint64_t)

struct pc_psw {
    uint64_t pc;  /* program counter (u64_t -> uint64_t) */
    uint64_t psw; /* processor status word (u64_t -> uint64_t) */
};

struct sig_info {
    int signo;
    struct pc_psw sigpcpsw;
};
