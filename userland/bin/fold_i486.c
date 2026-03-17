#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int width = 80;
    int fd = 0;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'w' && i + 1 < argc) {
            width = atoi(argv[++i]); if (width <= 0) width = 80;
        } else { fd = open(argv[i], O_RDONLY, 0); if (fd < 0) return 1; }
    }
    char ch; int col = 0;
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n') { write(1, "\n", 1); col = 0; continue; }
        if (col >= width) { write(1, "\n", 1); col = 0; }
        write(1, &ch, 1); ++col;
    }
    if (fd > 0) close(fd);
    return 0;
}
