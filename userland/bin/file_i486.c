#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: file path ...\n", 21); return 1; }
    for (int i = 1; i < argc; ++i) {
        write(1, argv[i], strlen(argv[i]));
        write(1, ": ", 2);
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) { write(1, "cannot open\n", 12); continue; }
        unsigned char buf[16]; ssize_t n = read(fd, buf, sizeof(buf)); close(fd);
        if (n >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') {
            write(1, "ELF", 3);
            if (n >= 5 && buf[4] == 1) write(1, " 32-bit", 7);
            else if (n >= 5 && buf[4] == 2) write(1, " 64-bit", 7);
            write(1, " executable\n", 12);
        } else if (n >= 2 && buf[0] == '#' && buf[1] == '!') {
            write(1, "script\n", 7);
        } else if (n >= 2 && buf[0] == 0x1F && buf[1] == 0x8B) {
            write(1, "gzip compressed data\n", 21);
        } else {
            int text = 1;
            for (ssize_t j = 0; j < n; ++j) {
                if (buf[j] == 0) { text = 0; break; }
            }
            write(1, text ? "ASCII text\n" : "data\n", text ? 11 : 5);
        }
    }
    return 0;
}
