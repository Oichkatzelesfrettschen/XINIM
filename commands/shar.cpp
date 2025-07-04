/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* shar - make a shell archive		Author: Michiel Husijes */

#include "blocksiz.hpp"

#define IO_SIZE (10 * BLOCK_SIZE)

char input[IO_SIZE];
char output[IO_SIZE];
int index = 0;

// Entry point with modern parameters
int main(int argc, char *argv[]) {
    register int i;
    int fd;

    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], 0)) < 0) {
            write(2, "Cannot open ", 12);
            write(2, argv[i], strlen(argv[i]));
            write(2, ".\n", 2);
        } else {
            print("echo x - ");
            print(argv[i]);
            print("\ngres '^X' '' > ");
            print(argv[i]);
            print(" << '/'\n");
            cat(fd);
        }
    }
    if (index)
        write(1, output, index);
    return 0;
}

static void cat(int fd) {
    static char *current, *last;
    register int r = 0;
    register char *cur_pos = current;

    putchar('X');
    for (;;) {
        if (cur_pos == last) {
            if ((r = read(fd, input, IO_SIZE)) <= 0)
                break;
            last = &input[r];
            cur_pos = input;
        }
        putchar(*cur_pos);
        if (*cur_pos++ == '\n' && cur_pos != last)
            putchar('X');
    }
    print("/\n");
    (void)close(fd);
    current = cur_pos;
}

static void print(char *str) {
    while (*str)
        putchar(*str++);
}

static void putchar(char c) {
    output[index++] = c;
    if (index == IO_SIZE) {
        write(1, output, index);
        index = 0;
    }
}
