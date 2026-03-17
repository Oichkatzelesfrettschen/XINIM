#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void write_num_padded(int fd, int n, int width) {
    char buf[12]; int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else { int v = n; while (v > 0) { buf[pos++] = (char)('0' + v % 10); v /= 10; } }
    for (int i = pos; i < width; ++i) write(fd, " ", 1);
    for (int i = pos - 1; i >= 0; --i) write(fd, buf + i, 1);
}

int main(int argc, char **argv) {
    int fd = 0;
    if (argc > 1) { fd = open(argv[1], O_RDONLY, 0); if (fd < 0) return 1; }
    int linenum = 1;
    char ch; int at_start = 1;
    while (read(fd, &ch, 1) == 1) {
        if (at_start) { write_num_padded(1, linenum++, 6); write(1, "\t", 1); at_start = 0; }
        write(1, &ch, 1);
        if (ch == '\n') at_start = 1;
    }
    if (fd > 0) close(fd);
    return 0;
}
