/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* echo - echo arguments	Author: Andy Tanenbaum */

#define SIZE 2048
char buf[SIZE];
int count;
/* Program entry point */
int main(int argc, char *argv[]) {
    register int i, nflag;

    nflag = 0;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n') {
        nflag++;
        argc--;
        argv++;
    }

    for (i = 1; i < argc; i++) {
        collect(argv[i]);
        if (i < argc - 1)
            collect(" ");
    }

    if (nflag == 0)
        collect("\n");

    /* Print accumulated output. */
    if (count > 0)
        write(1, buf, count);
    exit(0);
}

/* Accumulate output into buffer */
static void collect(char *s) {
    /* Collect characters.  For efficiency, write them in large chunks. */
    char c;

    if (count == SIZE) {
        write(1, buf, count);
        count = 0;
    }
    while ((c = *s++) != 0) {
        if (count < SIZE && c != '"')
            buf[count++] = c;
    }
}
