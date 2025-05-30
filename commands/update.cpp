/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* update - do sync periodically		Author: Andy Tanenbaum */

#include "signal.hpp"

main() {
    int fd, buf[2];

    /* Disable SIGTERM */
    signal(SIGTERM, SIG_IGN);

    /* Open some files to hold their inodes in core. */
    close(0);
    close(1);
    close(2);

    open("/bin", 0);
    open("/lib", 0);
    open("/etc", 0);
    open("/tmp", 0);

    /* Flush the cache every 30 seconds. */
    while (1) {
        sync();
        sleep(30);
    }
}
