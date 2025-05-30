#include "../include/lib.hpp" // C++17 header
#include "../include/stdio.hpp"

/*
 * Close a stream and release its resources.
 */
int fclose(FILE *fp) {
    int i; /* index within _io_table */

    for (i = 0; i < NFILES; i++) {
        if (fp == _io_table[i]) {
            _io_table[i] = 0;
            break;
        }
    }
    if (i >= NFILES)
        return EOF;
    fflush(fp);
    close(fp->_fd);
    if (testflag(fp, IOMYBUF) && fp->_buf)
        safe_free(fp->_buf);
    safe_free(fp);
    return 0;
}
