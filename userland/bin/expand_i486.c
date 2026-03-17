#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int fd = 0;
    if (argc > 1) { fd = open(argv[1], O_RDONLY, 0); if (fd < 0) return 1; }
    char ch; int col = 0;
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\t') {
            int spaces = 8 - (col % 8);
            for (int i = 0; i < spaces; ++i) { write(1, " ", 1); ++col; }
        } else if (ch == '\n') { write(1, "\n", 1); col = 0; }
        else { write(1, &ch, 1); ++col; }
    }
    if (fd > 0) close(fd);
    return 0;
}
