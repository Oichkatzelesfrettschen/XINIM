#include "../include/stdio.h"
#include <string.h>
#include <cstdint>  // For std::uintptr_t

/* Forward declarations for helper routines. */
#include "../include/shared/number_to_ascii.hpp"
// Forward declaration for the helper that handles padded output.
static void _printit(char *str, int w1, int w2, char padchar, int length, FILE *file);

// Use a constant for the maximum number of digits handled.
inline constexpr int kMaxDigits = 12;

// Convert an integer to a null-terminated ASCII string using the
// specified radix. The caller must ensure `a` has room for at least
// `kMaxDigits` characters plus the null terminator.
static void _bintoascii(long num, int radix, char *a) {
    // Buffer to build the number in reverse.
    char buf[kMaxDigits]{};
    int idx = 0;

    // Track negativity for decimal output.
    bool negative = false;
    if (radix == 10 && num < 0) {
        negative = true;
        num = -num;
    }

    // Special case for zero.
    if (num == 0) {
        a[0] = '0';
        a[1] = '\0';
        return;
    }

    // Generate digits in reverse order.
    while (num != 0 && idx < kMaxDigits - 1) {
        int digit = static_cast<int>(num % radix);
        buf[idx++] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10);
        num /= radix;
    }

    // Add a minus sign if needed.
    if (negative && idx < kMaxDigits - 1) {
        buf[idx++] = '-';
    }

    // Reverse the temporary buffer into the output string.
    for (int i = 0; i < idx; ++i) {
        a[i] = buf[idx - 1 - i];
    }
    a[idx] = '\0';
}

/* This is the same as varargs , on BSD systems */

#define GET_ARG(arglist, mode) ((mode *)(arglist += sizeof(mode)))[-1]

/* The main driver that handles formatted output similar to printf. */
[[maybe_unused]] static void _doprintf(FILE *fp, char *format, int args) {
    char *vl;
    int r, w1, w2, sign;
    long l;
    char c;
    char *s;
    char padchar;
    char a[kMaxDigits];

    vl = reinterpret_cast<char *>(static_cast<std::uintptr_t>(args));

    while (*format != '\0') {
        if (*format != '%') {
            putc(*format++, fp);
            continue;
        }

        w1 = 0;
        w2 = 0;
        sign = 1;
        padchar = ' ';
        format++;

        if (*format == '-') {
            sign = -1;
            format++;
        }

        if (*format == '0') {
            padchar = '0';
            format++;
        }

        while (*format >= '0' && *format <= '9') {
            w1 = 10 * w1 + sign * (*format - '0');
            format++;
        }

        if (*format == '.') {
            format++;
            while (*format >= '0' && *format <= '9') {
                w2 = 10 * w2 + (*format - '0');
                format++;
            }
        }

        switch (*format) {
        case 'd':
            l = (long)GET_ARG(vl, int);
            r = 10;
            break;
        case 'u':
            l = (long)GET_ARG(vl, int);
            l = l & 0xFFFF;
            r = 10;
            break;
        case 'o':
            l = (long)GET_ARG(vl, int);
            if (l < 0)
                l = l & 0xFFFF;
            r = 8;
            break;
        case 'x':
            l = (long)GET_ARG(vl, int);
            if (l < 0)
                l = l & 0xFFFF;
            r = 16;
            break;
        case 'D':
            l = (long)GET_ARG(vl, long);
            r = 10;
            break;
        case 'O':
            l = (long)GET_ARG(vl, long);
            r = 8;
            break;
        case 'X':
            l = (long)GET_ARG(vl, long);
            r = 16;
            break;
        case 'c':
            c = (char)GET_ARG(vl, int);
            /* char's are casted back to int's */
            putc(c, fp);
            format++;
            continue;
        case 's':
            s = GET_ARG(vl, char *);
            _printit(s, w1, w2, padchar, strlen(s), fp);
            format++;
            continue;
        default:
            putc('%', fp);
            putc(*format++, fp);
            continue;
        }

        _bintoascii(l, r, a);
        _printit(a, w1, w2, padchar, strlen(a), fp);
        format++;
    }
}

/* Output a formatted string with padding control. */
static void _printit(char *str, int w1, int w2, char padchar, int length, FILE *file) {
    int len2 = length;
    int temp;

    if (w2 > 0 && length > w2)
        len2 = w2;

    temp = len2;

    if (w1 > 0)
        while (w1 > len2) {
            --w1;
            putc(padchar, file);
        }

    while (*str && (len2-- != 0))
        putc(*str++, file);

    if (w1 < 0)
        if (padchar == '0') {
            putc('.', file);
            w1++;
        }
    while (w1 < -temp) {
        w1++;
        putc(padchar, file);
    }
}
