#include <cassert> // for assert
#include <cstring> // for strcmp
#include <fcntl.h>
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
    // Cast away const to match POSIX strcmp prototype.
    assert(strcmp(buf, const_cast<char *>(msg)) == 0);
    close(fd);
    unlink("tempfile");
    return 0;
}
