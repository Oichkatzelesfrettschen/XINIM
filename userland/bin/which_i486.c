#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: which command\n", 20); return 1; }
    const char *name = argv[1];

    /* If name contains '/', check directly */
    for (const char *p = name; *p; ++p) {
        if (*p == '/') {
            if (access(name, 1) == 0) { /* X_OK=1 */
                write(1, name, strlen(name));
                write(1, "\n", 1);
                return 0;
            }
            return 1;
        }
    }

    /* Search PATH */
    const char *path = getenv("PATH");
    if (path == 0) path = "/bin:/usr/bin";

    char buf[256];
    const char *p = path;
    while (*p) {
        const char *sep = p;
        while (*sep && *sep != ':') ++sep;
        size_t dlen = (size_t)(sep - p);
        size_t nlen = strlen(name);
        if (dlen + 1 + nlen + 1 <= sizeof(buf)) {
            memcpy(buf, p, dlen);
            buf[dlen] = '/';
            memcpy(buf + dlen + 1, name, nlen);
            buf[dlen + 1 + nlen] = '\0';
            if (access(buf, 1) == 0) {
                write(1, buf, strlen(buf));
                write(1, "\n", 1);
                return 0;
            }
        }
        p = (*sep) ? sep + 1 : sep;
    }
    return 1;
}
