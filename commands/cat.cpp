/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* cat - concatenates files  		Author: Andy Tanenbaum */

extern int errno; /*DEBUG*/

#include "blocksiz.hpp"
#include "stat.hpp"

#define BUF_SIZE 512   /* size of the output buffer */
int unbuffered;        /* non-zero for unbuffered operation */
char buffer[BUF_SIZE]; /* output buffer */
char *next = buffer;   /* next free byte in buffer */

/* Function prototypes */
static void copyfile(int fd1, int fd2);
static void flush(void);
static void quit(void);

int main(int argc, char *argv[]) {
    /*
     * Entry point.  Parse the command line and copy each file to
     * standard output.  The special file name '-' denotes standard
     * input.  The -u flag selects unbuffered operation.
     */
    int i, k, m, fd1;
    char *p;
    struct stat sbuf;

    k = 1;
    /* Check for the -u flag -- unbuffered operation. */
    p = argv[1];
    if (argc >= 2 && *p == '-' && *(p + 1) == 'u') {
        unbuffered = 1;
        k = 2;
    }

    if (k >= argc) {
        copyfile(0, 1);
        flush();
        exit(0);
    }

    for (i = k; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 0) {
            fd1 = 0;
        } else {
            fd1 = open(argv[i], 0);
            if (fd1 < 0) {
                std_err("cat: cannot open ");
                std_err(argv[i]);
                std_err("\n");
                continue;
            }
        }
        copyfile(fd1, 1);
        if (fd1 != 0)
            close(fd1);
    }
    flush();
    exit(0);
}

static void copyfile(int fd1, int fd2) {
    /*
     * Read data from fd1 and write it to fd2.  When running in buffered
     * mode the output is collected in 'buffer' and only written when the
     * buffer is full.  In unbuffered mode each read result is written
     * immediately.
     */
    int n, j, m;
    char buf[BLOCK_SIZE];

    while (1) {
        n = read(fd1, buf, BLOCK_SIZE);
        if (n < 0)
            quit();
        if (n == 0)
            return;
        if (unbuffered) {
            m = write(fd2, buf, n);
            if (m != n)
                quit();
        } else {
            for (j = 0; j < n; j++) {
                *next++ = buf[j];
                if (next == &buffer[BUF_SIZE]) {
                    m = write(fd2, buffer, BUF_SIZE);
                    if (m != BUF_SIZE)
                        quit();
                    next = buffer;
                }
            }
        }
    }
}

static void flush(void) {
    /* Write any buffered output to standard output. */
    if (next != buffer)
        if (write(1, buffer, next - buffer) <= 0)
            quit();
}

static void quit(void) {
    /* Terminate the program after printing the error stored in errno. */
    perror("cat");
    exit(1);
}
