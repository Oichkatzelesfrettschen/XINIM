/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

putc(ch, iop) char ch;
FILE *iop;
{
    int n, didwrite = 0;

    if (testflag(iop, (_ERR | _EOF)))
        return (EOF);

    if (!testflag(iop, WRITEMODE))
        return (EOF);

    if (testflag(iop, UNBUFF)) {
        n = write(iop->_fd, &ch, 1);
        iop->_count = 1;
        didwrite++;
    } else {
        *iop->_ptr++ = ch;
        if ((++iop->_count) >= BUFSIZ && !testflag(iop, STRINGS)) {
            n = write(iop->_fd, iop->_buf, iop->_count);
            iop->_ptr = iop->_buf;
            didwrite++;
        }
    }

    if (didwrite) {
        if (n <= 0 || iop->_count != n) {
            if (n < 0)
                iop->_flags |= _ERR;
            else
                iop->_flags |= _EOF;
            return (EOF);
        }
        iop->_count = 0;
    }
    return (0);
}
