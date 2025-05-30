/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* mktemp - make a name for a temporary file */

char *mktemp(template) char *template;
{
    int pid, k;
    char *p;

    pid = getpid(); /* get process id as semi-unique number */
    p = template;
    while (*p++)
        ; /* find end of string */
    p--;  /* backup to last character */

    /* Replace XXXXXX at end of template with pid. */
    while (*--p == 'X') {
        *p = '0' + (pid % 10);
        pid = pid / 10;
    }
    return (template);
}
