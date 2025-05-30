/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include <stdarg.h>
#include <stdio.hpp>

/*
 * Simplified scanf wrappers that delegate to the host C library.
 * These maintain the traditional interface while relying on the
 * platform implementation of the corresponding v* functions.
 */

int scanf(const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vscanf(format, ap);
    va_end(ap);
    return ret;
}

int fscanf(FILE *fp, const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vfscanf(fp, format, ap);
    va_end(ap);
    return ret;
}

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vsscanf(str, format, ap);
    va_end(ap);
    return ret;
}
