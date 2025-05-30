/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC int mount(special, name, rwflag)
char *name, *special;
int rwflag;
{
    return callm1(FS, MOUNT, len(special), len(name), rwflag, special, name, NIL_PTR);
}
