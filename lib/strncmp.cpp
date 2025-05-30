/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

int strncmp(s1, s2, n)
register char *s1, *s2;
int n;
{
    /* Compare two strings, but at most n characters. */

    while (1) {
        if (*s1 != *s2)
            return (*s1 - *s2);
        if (*s1 == 0 || --n == 0)
            return (0);
        s1++;
        s2++;
    }
}
