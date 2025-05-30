/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC long lseek(fd, offset, whence)
int fd;
long offset;
int whence;
{
    int k;
    M.m2_i1 = fd;
    M.m2_l1 = offset;
    M.m2_i2 = whence;
    k = callx(FS, LSEEK);
    if (k != OK)
        return ((long)k); /* send itself failed */
    return (M.m2_l1);
}
