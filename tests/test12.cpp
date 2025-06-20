#define _LARGEFILE64_SOURCE

/**
 * @file test12.cpp
 * @brief Validates large file support using 64-bit offsets.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief Entry point testing large file support.
 */
int main(void) {
    /* Name of the file to create */
    const char *fname = "bigfile";
    /* 4 GiB offset for large file testing */
    const off64_t off = (off64_t)4 * 1024 * 1024 * 1024;
    int fd;
    char c = 'x';
    struct stat64 st;

    printf("Test 12 ");

    /* Create and open the file for writing */
    fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* Seek to 4 GiB using 64-bit lseek */
    if (lseek64(fd, off, SEEK_SET) != off) {
        perror("lseek64");
        close(fd);
        return 1;
    }

    /* Write a single byte to extend the file */
    if (write(fd, &c, 1) != 1) {
        perror("write");
        close(fd);
        return 1;
    }
    close(fd);

    /* Verify file size with stat64 */
    if (stat64(fname, &st) != 0) {
        perror("stat64");
        return 1;
    }
    if (st.st_size != off + 1) {
        fprintf(stderr, "size mismatch\n");
        return 1;
    }

    /* Reopen for reading and verify lseek */
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (lseek64(fd, off, SEEK_SET) != off) {
        perror("lseek64");
        close(fd);
        return 1;
    }
    if (read(fd, &c, 1) != 1) {
        perror("read");
        close(fd);
        return 1;
    }
    if (c != 'x') {
        fprintf(stderr, "data mismatch\n");
        close(fd);
        return 1;
    }
    close(fd);

    unlink(fname);
    printf("ok\n");
    return 0;
}
