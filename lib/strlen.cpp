/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Compute the length of a string. */

#include <string.h>

/* Return the number of characters in the string s. */
size_t strlen(const char *s) {
    const char *original = s; /* keep starting position */

    /* Walk forward until the terminating null byte. */
    while (*s != '\0') {
        s++;
    }

    /* Difference gives the length. */
    return (size_t)(s - original);
}
