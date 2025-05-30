/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/defs.hpp"
#include <unistd.h>

/* Sector size in bytes. */
#define SECTOR_SIZE 512

/*
 * Read a sector from fd into buf. Returns 0 on success or -1 on failure.
 */
ssize_t absread(int fd, u64_t sector, void *buf) {
    off_t offset = (off_t)(sector * SECTOR_SIZE);
    ssize_t r = pread(fd, buf, SECTOR_SIZE, offset);

    if (r != SECTOR_SIZE)
        return -1;
    return 0;
}

/*
 * Write a sector from buf to fd. Returns 0 on success or -1 on failure.
 */
ssize_t abswrite(int fd, u64_t sector, const void *buf) {
    off_t offset = (off_t)(sector * SECTOR_SIZE);
    ssize_t r = pwrite(fd, buf, SECTOR_SIZE, offset);

    if (r != SECTOR_SIZE)
        return -1;
    return 0;
}

/* DMA overrun check stub for compatibility. */
int dmaoverr(void) { return 0; }
