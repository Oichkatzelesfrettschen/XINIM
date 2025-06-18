// Modernized for C++23

#include "errno.hpp"
#include "stat.hpp"
int no_creat = 0;

int main(int argc, char *argv[]) {
    char *path;
    int i = 1;

    if (argc == 1)
        usage();
    while (i < argc) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'f') {
                i += 1;
            } else if (argv[i][1] == 'c') {
                no_creat = 1;
                i += 1;
            } else {
                usage();
            }
        } else {
            path = argv[i];
            i += 1;
            if (doit(path) > 0) {
                std_err("touch: cannot touch ");
                std_err(path);
                std_err("\n");
            }
        }
    }
    exit(0);
}

static int doit(const char *name) {
    int fd;
    long *t, tim;
    struct stat buf;
    unsigned short tmp;
    long tvp[2];
    extern long time();

    if (!access(name, 0)) { /* change date if possible */
        stat(name, &buf);
        tmp = (buf.st_mode & S_IFREG);
        if (tmp != S_IFREG)
            return (1);

        tim = time(0L);
        tvp[0] = tim;
        tvp[1] = tim;
        if (!utime(name, tvp))
            return (0);
        else
            return (1);

    } else {
        /* file does not exist */
        if (no_creat == 1)
            return (0);
        else if ((fd = creat(name, 0777)) < 0) {
            return (1);
        } else {
            close(fd);
            return (0);
        }
    }
}

static void usage() {
    std_err("Usage: touch [-c] file...\n");
    exit(1);
}

static void std_err(const char *s) { prints("%s", s); }
