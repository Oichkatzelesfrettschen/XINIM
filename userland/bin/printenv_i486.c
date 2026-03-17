#include <unistd.h>
#include <string.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, char **argv) {
    if (argc > 1) {
        const char *val = getenv(argv[1]);
        if (val) { write(1, val, strlen(val)); write(1, "\n", 1); return 0; }
        return 1;
    }
    if (environ == 0) return 0;
    for (char **ep = environ; *ep; ++ep) {
        write(1, *ep, strlen(*ep));
        write(1, "\n", 1);
    }
    return 0;
}
