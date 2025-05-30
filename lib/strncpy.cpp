/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

char *strncpy(s1, s2, n)
register char *s1, *s2;
int n;
{
    /* Copy s2 to s1, but at most n characters. */

    char *original = s1;

    while (*s2 != 0) {
        *s1++ = *s2++;
        if (--n == 0)
            break;
    }
    *s1 = 0;
    return (original);
}
