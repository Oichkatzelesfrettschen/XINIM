/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

fwrite(ptr, size, count, file) unsigned size, count;
char *ptr;
FILE *file;
{
    unsigned s;
    unsigned ndone = 0;

    if (size)
        while (ndone < count) {
            s = size;
            do {
                putc(*ptr++, file);
                if (ferror(file))
                    return (ndone);
            } while (--s);
            ndone++;
        }
    return (ndone);
}
