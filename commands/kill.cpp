/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* kill - send a signal to a process	Author: Adri Koppes  */

#include "signal.hpp"

int main(int argc, char **argv) {
    int proc, signal = static_cast<int>(Signal::SIGTERM);

    if (argc < 2)
        usage();
    if (*argv[1] == '-') {
        signal = atoi(&argv[1][1]);
        *argv++;
        argc--;
    }
    if (!signal)
        signal = static_cast<int>(Signal::SIGTERM);
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
