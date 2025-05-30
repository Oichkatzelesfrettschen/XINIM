/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#ifndef KERNEL_TYPE_H
#define KERNEL_TYPE_H

/* The 'pc_psw' struct is machine dependent.  It must contain the information
 * pushed onto the stack by an interrupt, in the same format as the hardware
 * creates and expects.  It is used for storing the interrupt status after a
 * trap or interrupt, as well as for causing interrupts for signals.
 */

#include "../include/defs.hpp"
struct pc_psw {
    u64_t pc;  /* program counter */
    u64_t psw; /* processor status word */
};

struct sig_info {
    int signo;
    struct pc_psw sigpcpsw;
};

#endif /* KERNEL_TYPE_H */
