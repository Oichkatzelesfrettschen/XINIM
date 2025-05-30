/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* Exercise simple open/read/write/close syscalls. */
int main(void) {
    const char *msg = "hi";
    int fd = open("tempfile", O_RDWR | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0);
    assert(write(fd, msg, 2) == 2);
    lseek(fd, 0, SEEK_SET);
    char buf[3] = {0};
    assert(read(fd, buf, 2) == 2);
    assert(strcmp(buf, msg) == 0);
    close(fd);
    unlink("tempfile");
    return 0;
}
