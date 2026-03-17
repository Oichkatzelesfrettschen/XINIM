#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* paste: merge lines from files side by side */
int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: paste file1 [file2 ...]\n", 30); return 1; }
    int fds[16]; int nfds = 0;
    for (int i = 1; i < argc && nfds < 16; ++i) {
        int fd = (argv[i][0] == '-' && argv[i][1] == '\0') ? 0 : open(argv[i], O_RDONLY, 0);
        if (fd < 0) { write(2, "paste: cannot open file\n", 24); return 1; }
        fds[nfds++] = fd;
    }

    int done = 0;
    while (!done) {
        done = 1;
        for (int i = 0; i < nfds; ++i) {
            if (i > 0) write(1, "\t", 1);
            char ch;
            int got = 0;
            while (read(fds[i], &ch, 1) == 1) {
                if (ch == '\n') { got = 1; done = 0; break; }
                write(1, &ch, 1); got = 1; done = 0;
            }
            (void)got;
        }
        if (!done) write(1, "\n", 1);
    }
    for (int i = 0; i < nfds; ++i) if (fds[i] > 0) close(fds[i]);
    return 0;
}
