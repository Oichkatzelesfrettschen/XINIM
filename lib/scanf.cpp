#include <cstdarg>
#include <cstdio>

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
