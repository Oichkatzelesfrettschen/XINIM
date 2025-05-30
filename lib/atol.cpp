/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/ctype.hpppp"

long atol(s)
register char *s;
{
    register long total = 0;
    register unsigned digit;
    register minus = 0;

    while (isspace(*s))
        s++;
    if (*s == '-') {
        s++;
        minus = 1;
    }
    while ((digit = *s++ - '0') < 10) {
        total *= 10;
        total += digit;
    }
    return (minus ? -total : total);
}
