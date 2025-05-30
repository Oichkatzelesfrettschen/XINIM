/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

/* Flush all open stdio files */
void _cleanup(void) {
    register int i;

    for (i = 0; i < NFILES; i++)
        if (_io_table[i] != NULL)
            fflush(_io_table[i]);
}
