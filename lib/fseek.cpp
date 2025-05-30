/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

fseek(iop, offset, where) FILE *iop;
long offset;
{
    int count;
    long lseek();
    long pos;

    iop->_flags &= ~(_EOF | _ERR);
    /* Clear both the end of file and error flags */

    if (testflag(iop, READMODE)) {
        if (where < 2 && iop->_buf && !testflag(iop, UNBUFF)) {
            count = iop->_count;
            pos = offset;

            if (where == 0)
                pos += count - lseek(fileno(iop), 0L, 1) - 1;
            /*^^^ This caused the problem : - 1 corrected
                  it */
            else
                offset -= count;

            if (count > 0 && pos <= count && pos >= iop->_buf - iop->_ptr) {
                iop->_ptr += (int)pos;
                iop->_count -= (int)pos;
                return (0);
            }
        }
        pos = lseek(fileno(iop), offset, where);
        iop->_count = 0;
    } else if (testflag(iop, WRITEMODE)) {
        fflush(iop);
        pos = lseek(fileno(iop), offset, where);
    }
    return ((pos == -1) ? -1 : 0);
}
