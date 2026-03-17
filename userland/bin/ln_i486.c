#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    int symbolic = 0;
    int argi = 1;

    if (argi < argc && argv[argi][0] == '-' && argv[argi][1] == 's') {
        symbolic = 1;
        ++argi;
    }

    if (argc - argi < 2) {
        write(2, "usage: ln [-s] target linkname\n", 30);
        return 1;
    }

    const char *target = argv[argi];
    const char *linkname = argv[argi + 1];

    if (symbolic) {
        if (symlink(target, linkname) != 0) {
            write(2, "ln: symlink failed\n", 19);
            return 1;
        }
    } else {
        if (link(target, linkname) != 0) {
            write(2, "ln: link failed\n", 16);
            return 1;
        }
    }
    return 0;
}
