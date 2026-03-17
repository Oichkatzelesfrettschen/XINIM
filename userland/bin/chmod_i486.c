#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) { write(2, "usage: chmod mode file ...\n", 26); return 1; }
    unsigned int mode = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '7'; ++p)
        mode = mode * 8 + (unsigned)(*p - '0');
    int status = 0;
    for (int i = 2; i < argc; ++i) {
        if (chmod(argv[i], mode) != 0) {
            write(2, "chmod: failed for ", 18);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            status = 1;
        }
    }
    return status;
}
