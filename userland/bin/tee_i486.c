#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    int append = 0;
    int first_file = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'a') {
            append = 1;
            first_file = i + 1;
        } else if (argv[i][0] != '-') { break; }
        else first_file = i + 1;
    }

    int fds[16];
    int nfds = 0;
    for (int i = first_file; i < argc && nfds < 16; ++i) {
        int flags = O_WRONLY | O_CREAT;
        if (append) flags |= O_APPEND;
        else flags |= O_TRUNC;
        int fd = open(argv[i], flags, 0644);
        if (fd >= 0) fds[nfds++] = fd;
        else {
            write(2, "tee: cannot open ", 17);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
        }
    }

    char buf[512];
    ssize_t n;
    while ((n = read(0, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
        for (int i = 0; i < nfds; ++i) write(fds[i], buf, n);
    }

    for (int i = 0; i < nfds; ++i) close(fds[i]);
    return 0;
}
