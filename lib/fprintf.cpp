/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

fprintf(file, fmt, args) FILE *file;
char *fmt;
int args;
{
    _doprintf(file, fmt, &args);
    if (testflag(file, PERPRINTF))
        fflush(file);
}

printf(fmt, args) char *fmt;
int args;
{
    _doprintf(stdout, fmt, &args);
    if (testflag(stdout, PERPRINTF))
        fflush(stdout);
}
