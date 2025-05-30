/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC int read(fd, buffer, nbytes)
int fd;
char *buffer;
int nbytes;
{
    int n;
    n = callm1(FS, READ, fd, nbytes, 0, buffer, NIL_PTR, NIL_PTR);
    return (n);
}
