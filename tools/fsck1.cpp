/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/defs.hpp"
#include <stdio.hpp>
#include <stdlib.hpp>
#include <unistd.h>

/* External entry point to main provided by fsck.c */
extern int main(void);

/* File descriptor for disk operations. */
long drive_fd = 0;

/*
 * Program entry. This emulates the minimal assembly start which clears
 * the argument registers and jumps to main.
 */
void _start(void) {
    int ret = main();
    _exit(ret);
}

/* Character output wrapper used by standalone fsck. */
void putc(int c) { putchar(c); }

/* Character input wrapper used by standalone fsck. */
int getc(void) { return getchar(); }

/* Reset diskette -- does nothing on the host. */
int reset_diskette(void) { return 0; }

/*
 * Perform raw disk I/O using pread/pwrite. The count argument is in
 * sectors rather than bytes.
 */
int diskio(int rw, unsigned long sector, void *buf, unsigned long count) {
    off_t offset = (off_t)sector * 512;
    size_t bytes = count * 512;
    ssize_t r;

    if (rw)
        r = pwrite(drive_fd, buf, bytes, offset);
    else
        r = pread(drive_fd, buf, bytes, offset);

    if (r != (ssize_t)bytes)
        return -1;
    return 0;
}
