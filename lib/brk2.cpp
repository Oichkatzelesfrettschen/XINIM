/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

/* Request memory segment move */
PUBLIC void brk2(void) {
    char *p1, *p2;

    p1 = (char *)get_size();
    callm1(MM, BRK2, 0, 0, 0, p1, p2, NIL_PTR);
}
