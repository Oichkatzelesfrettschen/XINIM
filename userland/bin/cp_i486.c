#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void errmsg(const char *prefix, const char *path) {
    write(2, prefix, strlen(prefix));
    write(2, path, strlen(path));
    write(2, "\n", 1);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        write(2, "usage: cp source dest\n", 22);
        return 1;
    }
    int src = open(argv[1], O_RDONLY, 0);
    if (src < 0) {
        errmsg("cp: cannot open ", argv[1]);
        return 1;
    }
    int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        errmsg("cp: cannot create ", argv[2]);
        close(src);
        return 1;
    }
    char buf[512];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0) {
        const char *p = buf;
        ssize_t left = n;
        while (left > 0) {
            ssize_t w = write(dst, p, left);
            if (w <= 0) {
                errmsg("cp: write error to ", argv[2]);
                close(src);
                close(dst);
                return 1;
            }
            p += w;
            left -= w;
        }
    }
    close(src);
    close(dst);
    return n < 0 ? 1 : 0;
}
