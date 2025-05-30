/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/signal.hpp"
#include "../include/lib.hpp"

sighandler_t vectab[NR_SIGS]; /* array of functions to catch signals */

/* The definition of signal really should be
 *  PUBLIC int (*signal(signr, func))()
 * but some compilers refuse to accept this, even though it is correct.
 * The only thing to do if you are stuck with such a defective compiler is
 * change it to
 *  PUBLIC int *signal(signr, func)
 * and change ../h/signal.hpp accordingly.
 */

PUBLIC sighandler_t signal(int signr, sighandler_t func) {
    int r;
    sighandler_t old;

    old = vectab[signr - 1];
    vectab[signr - 1] = func;
    M.m6_i1 = signr;
    M.m6_f1 = ((func == SIG_IGN || func == SIG_DFL) ? func : begsig);
    r = callx(MM, SIGNAL);
    return ((r < 0 ? (sighandler_t)r : old));
}
