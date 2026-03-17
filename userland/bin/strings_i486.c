#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    int min_len = 4;
    int fd = 0;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && i + 1 < argc) {
            long v = 0; const char *p = argv[++i];
            while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
            if (v > 0) min_len = (int)v;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) { write(2, "strings: cannot open\n", 21); return 1; }
        }
    }
    char buf[4096]; int pos = 0;
    unsigned char ch;
    while (read(fd, &ch, 1) == 1) {
        if (ch >= 32 && ch < 127) {
            if (pos < (int)sizeof(buf) - 1) buf[pos++] = (char)ch;
        } else {
            if (pos >= min_len) { write(1, buf, pos); write(1, "\n", 1); }
            pos = 0;
        }
    }
    if (pos >= min_len) { write(1, buf, pos); write(1, "\n", 1); }
    if (fd > 0) close(fd);
    return 0;
}
