/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

PUBLIC int link(name, name2)
char *name, *name2;
{
    return callm1(FS, LINK, len(name), len(name2), 0, name, name2, NIL_PTR);
}
