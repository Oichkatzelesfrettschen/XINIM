#include "../include/stdio.hpp"

// Flush the buffer associated with the given FILE stream.
extern "C" int mnx_fflush(FILE *iop) {
    // Skip flushing if the stream is unbuffered or not opened for writing.
    if (testflag(iop, UNBUFF) || !testflag(iop, WRITEMODE))
        return 0;

    // Nothing to flush when no bytes are buffered.
    if (iop->_count <= 0)
        return 0;

    // Write the contents of the buffer to disk.
    int count = write(iop->_fd, iop->_buf, iop->_count);

    // On success reset buffer bookkeeping.
    if (count == iop->_count) {
        iop->_count = 0;
        iop->_ptr = iop->_buf;
        return count;
    }

    // Set the error flag on partial write and indicate failure.
    iop->_flags |= _ERR;
    return EOF;
}
