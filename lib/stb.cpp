/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* library routine for copying structs with unpleasant alignment */

__stb(n, f, t) register char *f, *t;
register int n;
{
    if (n > 0)
        do
            *t++ = *f++;
        while (--n);
}
