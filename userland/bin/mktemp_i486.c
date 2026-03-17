#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    unsigned pid = (unsigned)getpid();
    char path[64] = "/tmp/tmp.";
    int len = (int)strlen(path);
    /* Append PID as hex */
    for (int i = 7; i >= 0; --i) {
        unsigned d = (pid >> (i * 4)) & 0xF;
        path[len++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    }
    path[len] = '\0';
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        write(2, "mktemp: failed\n", 15);
        return 1;
    }
    close(fd);
    write(1, path, (size_t)len);
    write(1, "\n", 1);
    return 0;
}
