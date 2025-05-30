/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"
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
