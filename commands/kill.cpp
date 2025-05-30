/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* kill - send a signal to a process	Author: Adri Koppes  */

#include "../h/signal.hpp"

int main(int argc, char **argv) {
    int proc, signal = SIGTERM;

    if (argc < 2)
        usage();
    if (*argv[1] == '-') {
        signal = atoi(&argv[1][1]);
        *argv++;
        argc--;
    }
    if (!signal)
        signal = SIGTERM;
    while (--argc) {
        *argv++;
        proc = atoi(*argv);
        if (!proc && strcmp(*argv, "0"))
            usage();
        if (kill(proc, signal)) {
            prints("Kill: %s no such process\n", itoa(proc));
            exit(1);
        }
    }
    exit(0);
}

static void usage() {
    prints("usage: kill pid\n");
    exit(1);
}
