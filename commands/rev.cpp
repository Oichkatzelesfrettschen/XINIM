/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* rev - reverse an ASCII line	  Authors: Paul Polderman & Michiel Huisjes */

#include "blocksiz.hpp"

/*
 * Reverse the contents of each line of the input files specified on the
 * command line.  A file name of '-' denotes standard input.
 */

#ifndef NULL
#define NULL 0
#endif

#ifndef EOF
#define EOF ((char)-1)
#endif

int fd; /* File descriptor from file currently being read */

/* Function prototypes */
static void rev(void);
static int nextchar(void);

int main(int argc, char *argv[]) {
    register unsigned short i;

    if (argc == 1) { /* If no arguments given, take stdin as input */
        fd = 0;
        rev();
        exit(0);
    }
    for (i = 1; i < argc; i++) { /* Reverse each line in arguments */
        if ((fd = open(argv[i], 0)) < 0) {
            std_err("Cannot open ");
            std_err(argv[i]);
            std_err("\n");
            continue;
        }
        rev();
        close(fd);
    }
    exit(0);
}

/*
 * Read characters from the current input file, reverse the order of each
 * line and write the result to standard output.
 */
static void rev(void) {
    char output[BLOCK_SIZE];   /* Contains a reversed line */
    register unsigned short i; /* Index in output array */

    do {
        i = BLOCK_SIZE - 1;
        while ((output[i] = nextchar()) != '\n' && output[i] != EOF)
            i--;
        write(1, &output[i + 1], BLOCK_SIZE - 1 - i); /* Write reversed line*/
        if (output[i] == '\n')                        /* and write a '\n' */
            write(1, "\n", 1);
    } while (output[i] != EOF);
}

/* Buffer used for input */
char buf[BLOCK_SIZE];
/*
 * Provide buffered input from the current file descriptor.
 * When the buffer becomes empty the next block is read.
 */
static int nextchar(void) /* Does a sort of buffered I/O */
{
    static int n = 0; /* Read count */
    static int i;     /* Index in input buffer to next character */

    if (--n <= 0) { /* We've had this block. Read in next block */
        n = read(fd, buf, BLOCK_SIZE);
        i = 0; /* Reset index in array */
    }
    return ((n <= 0) ? EOF : buf[i++]); /* Return -1 on EOF */
}
