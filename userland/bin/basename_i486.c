#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: basename path [suffix]\n", 29);
        return 1;
    }
    const char *path = argv[1];
    size_t len = strlen(path);

    /* strip trailing slashes */
    while (len > 1 && path[len - 1] == '/') --len;

    /* find last component */
    const char *base = path;
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '/' && i + 1 < len) base = path + i + 1;
    }
    size_t base_len = len - (size_t)(base - path);

    /* strip suffix if provided */
    if (argc >= 3) {
        size_t slen = strlen(argv[2]);
        if (base_len > slen && strncmp(base + base_len - slen, argv[2], slen) == 0)
            base_len -= slen;
    }

    write(1, base, base_len);
    write(1, "\n", 1);
    return 0;
}
