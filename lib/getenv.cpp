/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#define NULL (char *)0
char *getenv(name)
register char *name;
{
    extern char **environ;
    register char **v = environ, *p, *q;

    while ((p = *v) != NULL) {
        q = name;
        while (*p++ == *q)
            if (*q++ == 0)
                continue;
        if (*(p - 1) != '=')
            continue;
        return (p);
    }
    return (0);
}
