/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* split - split a file		Author: Michiel Huisjes */

#include "blocksiz.hpp"

int cut_line = 1000;
int infile;
char out_file[100];
char *suffix;

// Entry point with modern parameters
int main(int argc, char *argv[]) {
    unsigned short i;

    out_file[0] = 'x';
    infile = -1;

    if (argc > 4)
        usage();
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] >= '0' && argv[i][1] <= '9' && cut_line == 1000)
                cut_line = atoi(argv[i]);
            else if (argv[i][1] == '\0' && infile == -1)
                infile = 0;
            else
                usage();
        } else if (infile == -1) {
            if ((infile = open(argv[i], 0)) < 0) {
                std_err("Cannot open input file.\n");
                exit(1);
            }
        } else
            strcpy(out_file, argv[i]);
    }
    if (infile == -1)
        infile = 0;
    strcat(out_file, "aa");
    for (suffix = out_file; *suffix; suffix++)
        ;
    suffix--;
    /* Appendix now points to last `a' of "aa". We have to decrement it by one */
    *suffix = 'a' - 1;
    split();
    return 0;
}

static void split() {
    char buf[BLOCK_SIZE];
    register char *index, *base;
    register int n;
    int fd;
    long lines = 0L;

    fd = newfile();
    while ((n = read(infile, buf, BLOCK_SIZE)) > 0) {
        base = index = buf;
        while (--n >= 0) {
            if (*index++ == '\n')
                if (++lines % cut_line == 0) {
                    if (write(fd, base, index - base) != index - base)
                        quit();
                    base = index;
                    close(fd);
                    fd = newfile();
                }
        }
        if (write(fd, base, index - base) != index - base)
            quit();
    }
}

static int newfile() {
    int fd;

    if (++*suffix > 'z') { /* Increment letter */
        *suffix = 'a';     /* Reset last letter */
        ++*(suffix - 1);   /* Previous letter must be incremented*/
        /* E.g. was `filename.az' */
        /* Now `filename.ba' */
    }
    if ((fd = creat(out_file, 0644)) < 0) {
        std_err("Cannot create new file.\n");
        exit(2);
    }
    return fd;
}

[[noreturn]] static void usage() {
    std_err("Usage: split [-n] [file [name]].\n");
    exit(1);
}

[[noreturn]] static void quit() {
    std_err("split: write error\n");
    exit(1);
}
