/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* tail - print the end of a file */

#include "stdio.hpp"
#define TRUE 1
#define FALSE 0
#define BLANK ' '
#define TAB '\t'
#define NEWL '\n'

int lines, chars;
char buff[BUFSIZ];

// Entry point with arguments using C++ style
int main(int argc, char *argv[]) {
    char *s;
    FILE *input, *fopen();
    int count;

    setbuf(stdout, buff);
    argc--;
    argv++;
    lines = TRUE;
    chars = FALSE;
    count = -10;

    if (argc == 0) {
        tail(stdin, count);
        done(0);
    }

    s = *argv;
    if (*s == '-' || *s == '+') {
        s++;
        if (*s >= '0' && *s <= '9') {
            count = stoi(*argv);
            s++;
            while (*s >= '0' && *s <= '9')
                s++;
        }
        if (*s == 'c') {
            chars = TRUE;
            lines = FALSE;
        } else if (*s != 'l' && *s != NULL) {
            fprintf(stderr, "tail: unknown option %c\n", *s);
            argc = 0;
        }
        argc--;
        argv++;
    }

    if (argc < 0) {
        fprintf(stderr, "Usage: tail [+/-[number][lc]] [files]\n");
        done(1);
    }

    if (argc == 0)
        tail(stdin, count);

    else if ((input = fopen(*argv, "r")) == NULL) {
        fprintf(stderr, "tail: can't open %s\n", *argv);
        done(1);
    } else {
        tail(input, count);
        fclose(input);
    }

    done(0);
    return 0; // Unreachable
}

/* stoi - convert string to integer */

// Convert string to integer
static int stoi(char *s) {
    int n, sign;

    while (*s == BLANK || *s == NEWL || *s == TAB)
        s++;

    sign = 1;
    if (*s == '+')
        s++;
    else if (*s == '-') {
        sign = -1;
        s++;
    }
    for (n = 0; *s >= '0' && *s <= '9'; s++)
        n = 10 * n + *s - '0';
    return (sign * n);
}

/* tail - print 'count' lines/chars */

#define INCR(p)                                                                                    \
    if (p >= end)                                                                                  \
        p = cbuf;                                                                                  \
    else                                                                                           \
        p++
#define BUF_SIZE 4098

char cbuf[BUF_SIZE];

// Print the last part of the input
static void tail(FILE *in, int goal) {
    int c, count;
    char *start, *finish, *end;

    count = 0;

    if (goal > 0) { /* skip */

        if (lines) /* lines */
            while ((c = getc(in)) != EOF) {
                if (c == NEWL)
                    count++;
                if (count >= goal)
                    break;
            }
        else /* chars */
            while (getc(in) != EOF) {
                count++;
                if (count >= goal)
                    break;
            }
        if (count >= goal)
            while ((c = getc(in)) != EOF)
                putc(c, stdout);
    }

    else { /* tail */

        goal = -goal;
        start = finish = cbuf;
        end = &cbuf[BUF_SIZE - 1];

        while ((c = getc(in)) != EOF) {
            *finish = c;
            INCR(finish);

            if (start == finish)
                INCR(start);
            if (!lines || c == NEWL)
                count++;

            if (count > goal) {
                count = goal;
                if (lines)
                    while (*start != NEWL)
                        INCR(start);
                INCR(start);
            }

        } /* end while */

        while (start != finish) {
            putc(*start, stdout);
            INCR(start);
        }

    } /* end else */

} /* end tail */

// Terminate the program, flushing stdio buffers
[[noreturn]] static void done(int n) {
    _cleanup(); /* flush stdio's internal buffers */
    exit(n);
}
