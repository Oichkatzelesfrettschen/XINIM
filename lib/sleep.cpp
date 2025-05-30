/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"
#include "../include/signal.hpp"

PRIVATE void alfun(int signum) { (void)signum; }
PUBLIC void sleep(int n) {
    /* sleep(n) pauses for 'n' seconds by scheduling an alarm interrupt. */
    signal(SIGALRM, alfun);
    alarm(n);
    pause();
}
