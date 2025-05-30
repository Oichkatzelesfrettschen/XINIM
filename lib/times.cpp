/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

struct tbuf {
    long b1, b2, b3, b4;
};
PUBLIC int times(buf)
struct tbuf *buf;
{
    int k;
    k = callm1(FS, TIMES, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    buf->b1 = M.m4_l1;
    buf->b2 = M.m4_l2;
    buf->b3 = M.m4_l3;
    buf->b4 = M.m4_l4;
    return (k);
}
