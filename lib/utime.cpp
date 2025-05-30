/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC int utime(name, timp)
char *name;
long timp[2];
{
    M.m2_i1 = len(name);
    M.m2_l1 = timp[0];
    M.m2_l2 = timp[1];
    M.m2_p1 = name;
    return callx(FS, UTIME);
}
