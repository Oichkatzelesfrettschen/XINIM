/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

ungetc(ch, iop) int ch;
FILE *iop;
{
    if (ch < 0 || !testflag(iop, READMODE) || testflag(iop, UNBUFF))
        return (EOF);

    if (iop->_count >= BUFSIZ)
        return (EOF);

    if (iop->_ptr == iop->_buf)
        iop->_ptr++;

    iop->_count++;
    *--iop->_ptr = ch;
    return (ch);
}
