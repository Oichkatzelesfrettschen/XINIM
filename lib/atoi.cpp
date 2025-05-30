/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Implementation of the standard atoi() function. */

/* Local character classification helpers. */
#define isascii(c) (((unsigned)(c) & 0xFF) < 0200)

/* For all the following functions the parameter must be ASCII. */
#define _between(a, b, c) ((unsigned)((b) - (a)) < (c) - (a))

#define isupper(c) _between('A', c, 'Z')
#define islower(c) _between('a', c, 'z')
#define isdigit(c) _between('0', c, '9')
#define isprint(c) _between(' ', c, '~')

/* The others are problematic as the parameter may only be evaluated once. */
static _c_; /* used to store the evaluated parameter */

#define isalpha(c) (isupper(_c_ = (c)) || islower(_c_))
#define isalnum(c) (isalpha(c) || isdigit(_c_))
#define _isblank(c) ((_c_ = (c)) == ' ' || _c_ == '\t')
#define isspace(c) (_isblank(c) || _c_ == '\r' || _c_ == '\n' || _c_ == '\f')
#define iscntrl(c) ((_c_ = (c)) == 0177 || _c_ < ' ')

#include <stdlib.hpp>

/* Convert a numeric string to an integer. */
int atoi(const char *s) {
    int total = 0;  /* running total */
    unsigned digit; /* current digit */
    int minus = 0;  /* track a leading '-' */

    /* Skip leading whitespace characters. */
    while (isspace(*s)) {
        s++;
    }

    /* Handle optional leading minus sign. */
    if (*s == '-') {
        s++;
        minus = 1;
    }

    /* Process digits until a non-digit is encountered. */
    while ((digit = *s++ - '0') < 10) {
        total *= 10;
        total += digit;
    }

    return minus ? -total : total;
}
