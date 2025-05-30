/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC int close(fd)
int fd;
{
    return callm1(FS, CLOSE, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
