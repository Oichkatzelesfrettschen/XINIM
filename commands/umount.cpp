/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* umount - unmount a file system		Author: Andy Tanenbaum */

#include "errno.hpp"

extern int errno;

// Entry point with modern signature
int main(int argc, char *argv[]) {

    if (argc != 2)
        usage();
    if (umount(argv[1]) < 0) {
        if (errno == ErrorCode::EINVAL)
            std_err("Device not mounted\n");
        else
            perror("umount");
        exit(1);
    }
    std_err(argv[1]);
    std_err(" unmounted\n");
    return 0;
}

// Display usage message and terminate
[[noreturn]] static void usage() {
    std_err("Usage: umount special\n");
    exit(1);
}
