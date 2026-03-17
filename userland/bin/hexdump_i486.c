#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void write_hex_byte(int fd, unsigned char b) {
    char hex[2];
    hex[0] = "0123456789abcdef"[b >> 4];
    hex[1] = "0123456789abcdef"[b & 0xF];
    write(fd, hex, 2);
}

static void write_hex32(int fd, unsigned long v) {
    for (int i = 7; i >= 0; --i) {
        char c = "0123456789abcdef"[(v >> (i * 4)) & 0xF];
        write(fd, &c, 1);
    }
}

static int hexdump_fd(int fd) {
    unsigned char buf[16];
    unsigned long offset = 0;
    ssize_t n;

    while ((n = read(fd, buf, 16)) > 0) {
        write_hex32(1, offset);
        write(1, "  ", 2);

        for (ssize_t i = 0; i < 16; ++i) {
            if (i < n) { write_hex_byte(1, buf[i]); write(1, " ", 1); }
            else write(1, "   ", 3);
            if (i == 7) write(1, " ", 1);
        }

        write(1, " |", 2);
        for (ssize_t i = 0; i < n; ++i) {
            char c = (buf[i] >= 0x20 && buf[i] <= 0x7E) ? (char)buf[i] : '.';
            write(1, &c, 1);
        }
        write(1, "|\n", 2);
        offset += (unsigned long)n;
    }
    write_hex32(1, offset);
    write(1, "\n", 1);
    return 0;
}

int main(int argc, char **argv) {
    if (argc <= 1) return hexdump_fd(0);
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write(2, "hexdump: cannot open ", 21);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            continue;
        }
        hexdump_fd(fd);
        close(fd);
    }
    return 0;
}
