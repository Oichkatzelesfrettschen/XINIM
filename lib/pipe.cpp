/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC int pipe(fild)
int fild[2];
{
    int k;
    k = callm1(FS, PIPE, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k >= 0) {
        fild[0] = M.m1_i1;
        fild[1] = M.m1_i2;
        return (0);
    } else
        return (k);
}
