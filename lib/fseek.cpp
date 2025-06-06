// clang-format off
#include <unistd.h>
#include "../include/stdio.hpp"
// clang-format on

// Seek to a new position in the given stream.
int fseek(FILE *iop, long offset, int where) {
    iop->_flags &= ~(_EOF | _ERR); // clear end-of-file and error flags

    long pos = 0;
    if (testflag(iop, READMODE)) {
        if (where < 2 && iop->_buf && !testflag(iop, UNBUFF)) {
            int count = iop->_count;
            long p = offset;

            if (where == 0)
                p += count - lseek(fileno(iop), 0L, 1) - 1;
            else
                offset -= count;

            if (count > 0 && p <= count && p >= iop->_buf - iop->_ptr) {
                iop->_ptr += static_cast<int>(p);
                iop->_count -= static_cast<int>(p);
                return 0;
            }
        }
        pos = lseek(fileno(iop), offset, where);
        iop->_count = 0;
    } else if (testflag(iop, WRITEMODE)) {
        fflush(iop);
        pos = lseek(fileno(iop), offset, where);
    }
    return (pos == -1) ? -1 : 0;
}
