/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

long ftell(iop)
FILE *iop;
{
    long result;
    long lseek();
    int adjust = 0;

    if (testflag(iop, READMODE))
        adjust -= iop->_count;
    else if (testflag(iop, WRITEMODE) && iop->_buf && !testflag(iop, UNBUFF))
        adjust = iop->_ptr - iop->_buf;
    else
        return (-1);

    result = lseek(fileno(iop), 0L, 1);

    if (result < 0)
        return (result);

    result += (long)adjust;
    return (result);
}
