/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

char *gets(str)
char *str;
{
    register int ch;
    register char *ptr;

    ptr = str;
    while ((ch = getc(stdin)) != EOF && ch != '\n')
        *ptr++ = ch;

    if (ch == EOF && ptr == str)
        return (NULL);
    *ptr = '\0';
    return (str);
}
