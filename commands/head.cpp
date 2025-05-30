/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* head - print the first few lines of a file	Author: Andy Tanenbaum */

#include "stdio.hpp"

#define DEFAULT 10

char buff[BUFSIZ];
char lbuf[256];

int main(int argc, char *argv[]) {

    int n, k, nfiles;
    char *ptr;

    /* Check for flag.  Only flag is -n, to say how many lines to print. */
    setbuf(stdout, buff);
    k = 1;
    ptr = argv[1];
    n = DEFAULT;
    if (*ptr++ == '-') {
        k++;
        n = atoi(ptr);
        if (n <= 0)
            usage();
    }
    nfiles = argc - k;

    if (nfiles == 0) {
        /* Print standard input only. */
        do_file(n);
        fflush(stdout);
        exit(0);
    }

    /* One or more files have been listed explicitly. */
    while (k < argc) {
        fclose(stdin);
        if (nfiles > 1)
            prints("==> %s <==\n", argv[k]);
        if (fopen(argv[k], "r") == NULL)
            prints("head: cannot open %s\n", argv[k]);
        else
            do_file(n);
        k++;
        if (k < argc)
            prints("\n");
    }
    fflush(stdout);
    exit(0);
}

static void do_file(int n) {
    /* Print the first 'n' lines of a file. */
    while (n--)
        do_line();
}

static void do_line() {
    /* Print one line. */

    char c, *cp;
    cp = lbuf;
    while ((c = getc(stdin)) != '\n')
        *cp++ = c;
    *cp++ = '\n';
    *cp++ = 0;
    prints("%s", lbuf);
}

static void usage() {
    std_err("Usage: head [-n] [file ...]\n");
    exit(1);
}
