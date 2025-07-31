/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header
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
