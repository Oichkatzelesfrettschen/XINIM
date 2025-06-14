#include "../include/stdio.hpp"

/**
 * @brief Flush the buffer associated with the given stream.
 *
 * @param iop Stream to flush.
 * @return Bytes written or ::STDIO_EOF on error.
 */
int __fflush(FILE *iop) {
    int count;

    if (testflag(iop, UNBUFF) || !testflag(iop, WRITEMODE))
        return 0;

    if (iop->_count <= 0)
        return 0;

    count = write(iop->_fd, iop->_buf, iop->_count);

    if (count == iop->_count) {
        iop->_count = 0;
        iop->_ptr = iop->_buf;
        return count;
    }

    iop->_flags |= ERR;
    return STDIO_EOF;
}

/**
 * @brief Public interface wrapper for ::__fflush.
 *
 * @param stream Stream to flush.
 * @return Bytes written or ::STDIO_EOF on error.
 */
int fflush(FILE *stream) { return __fflush(stream); }
