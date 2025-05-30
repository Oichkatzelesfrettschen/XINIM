/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

extern char *brksize;

PUBLIC char *brk(addr)
char *addr;
{
    int k;

    k = callm1(MM, BRK, 0, 0, 0, addr, NIL_PTR, NIL_PTR);
    if (k == OK) {
        brksize = M.m2_p1;
        return (NIL_PTR);
    } else {
        return ((char *)-1);
    }
}

PUBLIC char *sbrk(incr)
char *incr;
{
    char *newsize, *oldsize;
    extern int endv, dorgv;

    oldsize = brksize;
    newsize = brksize + (int)incr;
    if (brk(newsize) == 0)
        return (oldsize);
    else
        return ((char *)-1);
}
