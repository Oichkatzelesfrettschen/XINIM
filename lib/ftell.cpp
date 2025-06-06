// clang-format off
#include <unistd.h>
#include "../include/stdio.hpp"
// clang-format on

// Return the current file position of the stream.
long ftell(FILE *iop) {
    int adjust = 0;
    if (testflag(iop, READMODE))
        adjust -= iop->_count;
    else if (testflag(iop, WRITEMODE) && iop->_buf && !testflag(iop, UNBUFF))
        adjust = iop->_ptr - iop->_buf;
    else
        return -1;

    long result = lseek(fileno(iop), 0L, 1);
    if (result < 0)
        return result;
    result += static_cast<long>(adjust);
    return result;
}
