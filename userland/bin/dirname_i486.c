#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: dirname path\n", 20);
        return 1;
    }
    const char *path = argv[1];
    size_t len = strlen(path);

    /* strip trailing slashes */
    while (len > 1 && path[len - 1] == '/') --len;

    /* strip last component */
    while (len > 0 && path[len - 1] != '/') --len;

    /* strip trailing slashes again */
    while (len > 1 && path[len - 1] == '/') --len;

    if (len == 0) {
        write(1, ".\n", 2);
    } else {
        write(1, path, len);
        write(1, "\n", 1);
    }
    return 0;
}
