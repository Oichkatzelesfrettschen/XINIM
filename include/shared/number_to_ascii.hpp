#pragma once

#include <cstddef>

// Convert a number to an ASCII representation in the given radix.
// The result is written into `out` and the number of characters
// written is returned. The caller must ensure that `out` has
// enough space for the digits and optional sign.
static inline int number_to_ascii(long num, int radix, char *out) {
    constexpr int kMaxDigits = 12;
    char buf[kMaxDigits];
    int hit = 0;
    int negative = 0;
    int n = 0;

    if (num == 0) {
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }

    if (radix == 10 && num < 0) {
        num = -num;
        negative = 1;
    }

    for (int i = 0; i < kMaxDigits; ++i) {
        buf[i] = 0;
    }

    do {
        if (radix == 10) {
            buf[n] = num % 10;
            num = (num - buf[n]) / 10;
        }
        if (radix == 8) {
            buf[n] = num & 0x7;
            num = (num >> 3) & 0x1FFFFFFF;
        }
        if (radix == 16) {
            buf[n] = num & 0xF;
            num = (num >> 4) & 0x0FFFFFFF;
        }
        ++n;
    } while (num != 0);

    for (int i = n - 1; i >= 0; --i) {
        if (buf[i] == 0 && hit == 0) {
            buf[i] = ' ';
        } else {
            if (buf[i] < 10)
                buf[i] += '0';
            else
                buf[i] += 'A' - 10;
            ++hit;
        }
    }
    if (negative)
        buf[n++] = '-';

    int out_len = n;
    for (int i = n - 1; i >= 0; --i) {
        *out++ = buf[i];
    }
    *out = '\0';
    return out_len;
}
