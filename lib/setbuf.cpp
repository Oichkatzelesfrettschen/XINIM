#include "../include/lib.hpp" // C++23 header
#include "../include/stdio.hpp"

void setbuf(FILE *iop, char *buffer) {
    if (iop->_buf && testflag(iop, IOMYBUF))
        safe_free(iop->_buf);

    iop->_flags &= ~(IOMYBUF | UNBUFF | PERPRINTF);

    iop->_buf = buffer;

    if (iop->_buf == NULL)
        iop->_flags |= UNBUFF;

    iop->_ptr = iop->_buf;
    iop->_count = 0;
}
