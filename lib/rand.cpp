/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Simple linear congruential random number generator. */

#include <stdlib.hpp>

/* Seed value used by rand(). */
static long seed = 1L;

/* Return a pseudo-random integer in the range [0, 32767]. */
int rand(void) {
    seed = (1103515245L * seed + 12345) & 0x7FFFFFFF;
    return (int)(seed & 077777);
}
