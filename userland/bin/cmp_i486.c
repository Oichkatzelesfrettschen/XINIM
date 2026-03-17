#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void write_num(int fd, unsigned long n) {
    char buf[20]; int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = 0; i < pos/2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; }
    write(fd, buf, pos);
}

int main(int argc, char **argv) {
    int silent = 0;
    int argi = 1;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 's') { silent = 1; ++argi; }
    if (argc - argi < 2) { write(2, "usage: cmp [-s] file1 file2\n", 28); return 2; }

    int fd1 = open(argv[argi], O_RDONLY, 0);
    int fd2 = open(argv[argi+1], O_RDONLY, 0);
    if (fd1 < 0 || fd2 < 0) { if (!silent) write(2, "cmp: cannot open file\n", 22); return 2; }

    unsigned long byte = 1, line = 1;
    unsigned char c1, c2;
    for (;;) {
        ssize_t r1 = read(fd1, &c1, 1), r2 = read(fd2, &c2, 1);
        if (r1 == 0 && r2 == 0) { close(fd1); close(fd2); return 0; }
        if (r1 == 0 || r2 == 0) {
            if (!silent) { write(1, "cmp: EOF on ", 12); write(1, r1==0 ? argv[argi] : argv[argi+1], strlen(r1==0 ? argv[argi] : argv[argi+1])); write(1, "\n", 1); }
            close(fd1); close(fd2); return 1;
        }
        if (c1 != c2) {
            if (!silent) {
                write(1, argv[argi], strlen(argv[argi])); write(1, " ", 1);
                write(1, argv[argi+1], strlen(argv[argi+1]));
                write(1, " differ: byte ", 14); write_num(1, byte);
                write(1, ", line ", 7); write_num(1, line); write(1, "\n", 1);
            }
            close(fd1); close(fd2); return 1;
        }
        if (c1 == '\n') ++line;
        ++byte;
    }
}
