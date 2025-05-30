/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "signal.hpp"
extern int errno;
int testnr;
int errct;

int zilch[5000];

main() {
    int i;

    printf("Test  7 ");
    for (i = 0; i < 150; i++) {
        test70();
    }
    if (errct == 0)
        printf("ok\n");
    else
        printf("%d errors\n", errct);
}

test70() {
    int i, err, pid;

    signal(SIGQUIT, SIG_IGN);
    err = 0;
    for (i = 0; i < 5000; i++)
        if (zilch[i] != 0)
            err++;
    if (err > 0)
        e(1);
    kill(getpid(), SIGQUIT);
}

e(n) int n;
{
    printf("Subtest %d,  error %d  errno=%d  ", testnr, n, errno);
    perror("");
    errct++;
}
