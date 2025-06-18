/* This is a special version of printf.  It is used only by the operating
 * system itself, and should never be included in user programs.   The name
 * printk never appears in the operating system, because the macro printf
 * has been defined as printk there.
 */

#define MAXDIGITS 12

#include "../include/shared/number_to_ascii.hpp"

static int bintoascii(long num, int radix, char a[MAXDIGITS]);

void printk(char *s, int *arglist) {
    int w, k, r, *valp;
    unsigned u;
    long l, *lp;
    char a[MAXDIGITS], *p, *p1, c;

    valp = (int *)&arglist;
    while (*s != '\0') {
        if (*s != '%') {
            putc(*s++);
            continue;
        }

        w = 0;
        s++;
        while (*s >= '0' && *s <= '9') {
            w = 10 * w + (*s - '0');
            s++;
        }

        lp = (long *)valp;

        switch (*s) {
        case 'd':
            k = *valp++;
            l = k;
            r = 10;
            break;
        case 'o':
            k = *valp++;
            u = k;
            l = u;
            r = 8;
            break;
        case 'x':
            k = *valp++;
            u = k;
            l = u;
            r = 16;
            break;
        case 'D':
            l = *lp++;
            r = 10;
            valp = (int *)lp;
            break;
        case 'O':
            l = *lp++;
            r = 8;
            valp = (int *)lp;
            break;
        case 'X':
            l = *lp++;
            r = 16;
            valp = (int *)lp;
            break;
        case 'c':
            k = *valp++;
            putc(k);
            s++;
            continue;
        case 's':
            p = (char *)*valp++;
            p1 = p;
            while ((c = *p++) != '\0')
                putc(c);
            s++;
            if ((k = w - (p - p1 - 1)) > 0)
                while (k--)
                    putc(' ');
            continue;
        default:
            putc('%');
            putc(*s++);
            continue;
        }

        k = bintoascii(l, r, a);
        if ((r = w - k) > 0)
            while (r--)
                putc(' ');
        for (r = k - 1; r >= 0; r--)
            putc(a[r]);
        s++;
    }
}

static int bintoascii(long num, int radix, char a[MAXDIGITS]) {
    return number_to_ascii(num, radix, a);
}
