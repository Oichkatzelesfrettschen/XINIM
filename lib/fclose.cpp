/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header
#include "../include/stdio.hpp"
#include <unistd.h>

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
