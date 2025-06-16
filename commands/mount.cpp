/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

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
        if (errno == ErrorCode::EINVAL) {
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
