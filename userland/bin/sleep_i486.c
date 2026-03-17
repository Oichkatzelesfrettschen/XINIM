#include <unistd.h>
#include <time.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: sleep seconds\n", 21);
        return 1;
    }
    long secs = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; ++p)
        secs = secs * 10 + (*p - '0');
    if (secs <= 0) return 0;

    struct timespec req;
    req.tv_sec = secs;
    req.tv_nsec = 0;
    nanosleep(&req, 0);
    return 0;
}
