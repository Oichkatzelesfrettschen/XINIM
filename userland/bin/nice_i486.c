#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    int adj = 10, argi = 1;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n' && argc > 2) {
        long v = 0; const char *p = argv[2];
        int neg = 0; if (*p == '-') { neg = 1; ++p; }
        while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        adj = neg ? -(int)v : (int)v;
        argi = 3;
    }
    (void)adj; /* nice value ignored on single-user system */
    if (argi >= argc) { write(2, "usage: nice [-n adj] command\n", 29); return 1; }
    execve(argv[argi], argv + argi, 0);
    write(2, "nice: exec failed\n", 18);
    return 127;
}
