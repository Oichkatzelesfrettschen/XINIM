#include <unistd.h>
#include <stdint.h>

/* Sector size in bytes. */
#define SECTOR_SIZE 512

/*
 * Read a sector from fd into buf. Returns 0 on success or -1 on failure.
 */
ssize_t absread(int fd, uint64_t sector, void *buf)
{
    off_t offset = (off_t)(sector * SECTOR_SIZE);
    ssize_t r = pread(fd, buf, SECTOR_SIZE, offset);

    if (r != SECTOR_SIZE)
        return -1;
    return 0;
}

/*
 * Write a sector from buf to fd. Returns 0 on success or -1 on failure.
 */
ssize_t abswrite(int fd, uint64_t sector, const void *buf)
{
    off_t offset = (off_t)(sector * SECTOR_SIZE);
    ssize_t r = pwrite(fd, buf, SECTOR_SIZE, offset);

    if (r != SECTOR_SIZE)
        return -1;
    return 0;
}

/* DMA overrun check stub for compatibility. */
int dmaoverr(void)
{
    return 0;
}

