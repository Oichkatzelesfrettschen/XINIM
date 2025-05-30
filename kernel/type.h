#ifndef KERNEL_TYPE_H
#define KERNEL_TYPE_H

/* The 'pc_psw' struct is machine dependent.  It must contain the information
 * pushed onto the stack by an interrupt, in the same format as the hardware
 * creates and expects.  It is used for storing the interrupt status after a
 * trap or interrupt, as well as for causing interrupts for signals.
 */

#include <stdint.h>
struct pc_psw {
    uint64_t pc;  /* program counter */
    uint64_t psw; /* processor status word */
};

struct sig_info {
    int signo;
    struct pc_psw sigpcpsw;
};

#endif /* KERNEL_TYPE_H */
