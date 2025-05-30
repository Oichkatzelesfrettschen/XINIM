/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

fflush(iop) FILE *iop;
{
    int count;

    if (testflag(iop, UNBUFF) || !testflag(iop, WRITEMODE))
        return (0);

    if (iop->_count <= 0)
        return (0);

    count = write(iop->_fd, iop->_buf, iop->_count);

    if (count == iop->_count) {
        iop->_count = 0;
        iop->_ptr = iop->_buf;
        return (count);
    }

    iop->_flags |= _ERR;
    return (EOF);
}
