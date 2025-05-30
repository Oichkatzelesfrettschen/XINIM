/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC long time(tp)
long *tp;
{
    int k;
    long l;
    k = callm1(FS, TIME, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (M.m_type < 0 || k != OK) {
        errno = -M.m_type;
        return (-1L);
    }
    l = M.m2_l1;
    if (tp != (long *)0)
        *tp = l;
    return (l);
}
