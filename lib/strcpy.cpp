/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/*
 * strcpy - copy a string
 *
 * This routine copies the string pointed to by 'src' to the buffer pointed
 * to by 'dest'.  The terminating null byte is also copied.  A pointer to the
 * destination buffer is returned.
 */

char *strcpy(char *dest, const char *src) {
    /* keep start of destination so it can be returned */
    char *d = dest;

    /* copy each byte from src to dest, including final '\0' */
    while ((*dest++ = *src++) != '\0') {
        ; /* nothing */
    }

    /* return the original destination pointer */
    return d;
}
