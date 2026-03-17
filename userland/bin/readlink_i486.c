#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: readlink path\n", 21); return 1; }
    char buf[256];
    ssize_t n = readlink(argv[1], buf, sizeof(buf) - 1);
    if (n < 0) { write(2, "readlink: not a symlink\n", 24); return 1; }
    buf[n] = '\0';
    write(1, buf, (size_t)n);
    write(1, "\n", 1);
    return 0;
}
