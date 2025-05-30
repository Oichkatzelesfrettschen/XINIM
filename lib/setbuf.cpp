/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"
#include "../include/stdio.hpp"

setbuf(iop, buffer) FILE *iop;
char *buffer;
{
    if (iop->_buf && testflag(iop, IOMYBUF))
        safe_free(iop->_buf);

    iop->_flags &= ~(IOMYBUF | UNBUFF | PERPRINTF);

    iop->_buf = buffer;

    if (iop->_buf == NULL)
        iop->_flags |= UNBUFF;

    iop->_ptr = iop->_buf;
    iop->_count = 0;
}
