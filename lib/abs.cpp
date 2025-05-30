/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Implementation of the standard abs() function. */

#include <stdlib.hpp>

/* Return the absolute value of the integer argument. */
int abs(int i) {
    /* Use a simple ternary expression to select the correct sign. */
    return (i < 0) ? -i : i;
}
