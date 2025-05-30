/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

FILE *freopen(name, mode, stream)
char *name, *mode;
FILE *stream;
{
    FILE *fopen();

    if (fclose(stream) != 0)
        return NULL;

    return fopen(name, mode);
}
