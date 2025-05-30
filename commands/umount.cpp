/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* umount - unmount a file system		Author: Andy Tanenbaum */

#include "errno.hpp"

extern int errno;

main(argc, argv) int argc;
char *argv[];
{

    if (argc != 2)
        usage();
    if (umount(argv[1]) < 0) {
        if (errno == EINVAL)
            std_err("Device not mounted\n");
        else
            perror("umount");
        exit(1);
    }
    std_err(argv[1]);
    std_err(" unmounted\n");
    exit(0);
}

usage() {
    std_err("Usage: umount special\n");
    exit(1);
}
