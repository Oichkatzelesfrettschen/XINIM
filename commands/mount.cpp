/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* mount - mount a file system		Author: Andy Tanenbaum */

#include "errno.hpp"
extern int errno;

/* Program entry point */
int main(int argc, char *argv[]) {

    int ro;

    if (argc < 3 || argc > 4)
        usage();
    if (argc == 4 && *argv[3] != '-' && *(argv[3] + 1) != 'r')
        usage();
    ro = (argc == 4 ? 1 : 0);
    if (mount(argv[1], argv[2], ro) < 0) {
        if (errno == EINVAL) {
            std_err("mount: ");
            std_err(argv[1]);
            std_err(" is not a valid file system.\n");
        } else {
            perror("mount");
        }
        exit(1);
    }
    std_err(argv[1]);
    std_err(" mounted\n");
    exit(0);
}

/* Print usage information */
static void usage(void) {
    std_err("Usage: mount special name [-r]\n");
    exit(1);
}
