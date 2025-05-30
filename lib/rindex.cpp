/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

char *rindex(s, c)
register char *s, c;
{
    register char *result;

    result = 0;
    do
        if (*s == c)
            result = s;
    while (*s++ != 0);
    return (result);
}
