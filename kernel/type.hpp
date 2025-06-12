#pragma once
// Modernized for C++17

/* The 'pc_psw' struct is machine dependent.  It must contain the information
 * pushed onto the stack by an interrupt, in the same format as the hardware
 * creates and expects.  It is used for storing the interrupt status after a
 * trap or interrupt, as well as for causing interrupts for signals.
 */

#include "../include/defs.hpp" // Project-wide definitions
#include "../../include/xinim/core_types.hpp" // For xinim::virt_addr_t and std::uint64_t

struct pc_psw {
    xinim::virt_addr_t pc;  /* program counter */
    std::uint64_t psw;      /* processor status word */

};

struct sig_info {
    int signo;
    struct pc_psw sigpcpsw;
};
