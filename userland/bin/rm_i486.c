#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: rm file ...\n", 19);
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        if (unlink(argv[i]) != 0) {
            write(2, "rm: cannot remove ", 18);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            status = 1;
        }
    }
    return status;
}
