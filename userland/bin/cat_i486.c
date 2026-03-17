#include <fcntl.h>
#include <unistd.h>

static int cat_fd(int fd) {
    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        const char *p = buf;
        ssize_t left = n;
        while (left > 0) {
            ssize_t w = write(1, p, left);
            if (w <= 0) return 1;
            p += w;
            left -= w;
        }
    }
    return n < 0 ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc <= 1)
        return cat_fd(0);
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write(2, "cat: ", 5);
            const char *p = argv[i];
            while (*p) ++p;
            write(2, argv[i], p - argv[i]);
            write(2, ": not found\n", 12);
            status = 1;
            continue;
        }
        if (cat_fd(fd) != 0) status = 1;
        close(fd);
    }
    return status;
}
