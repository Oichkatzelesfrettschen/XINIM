/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

/* Get a character from stream */
int getc(FILE *iop) {
    int ch;

    if (testflag(iop, (_EOF | _ERR)))
        return (EOF);

    if (!testflag(iop, READMODE))
        return (EOF);

    if (--iop->_count <= 0) {

        if (testflag(iop, UNBUFF))
            iop->_count = read(iop->_fd, &ch, 1);
        else
            iop->_count = read(iop->_fd, iop->_buf, BUFSIZ);

        if (iop->_count <= 0) {
            if (iop->_count == 0)
                iop->_flags |= _EOF;
            else
                iop->_flags |= _ERR;

            return (EOF);
        } else
            iop->_ptr = iop->_buf;
    }

    if (testflag(iop, UNBUFF))
        return (ch & CMASK);
    else
        return (*iop->_ptr++ & CMASK);
}
