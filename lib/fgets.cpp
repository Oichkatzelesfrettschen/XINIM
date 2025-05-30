/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

char *fgets(str, n, file)
char *str;
unsigned n;
FILE *file;
{
    register int ch;
    register char *ptr;

    ptr = str;
    while (--n > 0 && (ch = getc(file)) != EOF) {
        *ptr++ = ch;
        if (ch == '\n')
            break;
    }
    if (ch == EOF && ptr == str)
        return (NULL);
    *ptr = '\0';
    return (str);
}
