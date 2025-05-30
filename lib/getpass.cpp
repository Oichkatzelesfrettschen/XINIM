/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../h/sgtty.hpp"

static char pwdbuf[9];

char *getpass(prompt)
char *prompt;
{
    int i = 0;
    struct sgttyb tty;

    prints(prompt);
    ioctl(0, TIOCGETP, &tty);
    tty.sg_flags = 06020;
    ioctl(0, TIOCSETP, &tty);
    i = read(0, pwdbuf, 9);
    while (pwdbuf[i - 1] != '\n')
        read(0, &pwdbuf[i - 1], 1);
    pwdbuf[i - 1] = '\0';
    tty.sg_flags = 06030;
    ioctl(0, TIOCSETP, &tty);
    prints("\n");
    return (pwdbuf);
}
