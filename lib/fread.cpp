/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

fread(ptr, size, count, file) char *ptr;
unsigned size, count;
FILE *file;
{
    register int c;
    unsigned ndone = 0, s;

    ndone = 0;
    if (size)
        while (ndone < count) {
            s = size;
            do {
                if ((c = getc(file)) != EOF)
                    *ptr++ = (char)c;
                else
                    return (ndone);
            } while (--s);
            ndone++;
        }
    return (ndone);
}
