#include <unistd.h>
#include <string.h>

/* Minimal printf(1) utility -- supports %s, %d, %x, %%, \n, \t, \\ */

static void out(const char *s, size_t len) {
    while (len > 0) {
        ssize_t w = write(1, s, len);
        if (w <= 0) return;
        s += w;
        len -= (size_t)w;
    }
}

static void out_str(const char *s) {
    out(s, strlen(s));
}

static void out_char(char c) {
    out(&c, 1);
}

static void out_int(long val) {
    char buf[24];
    int neg = 0;
    unsigned long uval;
    if (val < 0) { neg = 1; uval = (unsigned long)(-val); }
    else uval = (unsigned long)val;

    int pos = 0;
    if (uval == 0) { buf[pos++] = '0'; }
    else { while (uval > 0) { buf[pos++] = (char)('0' + uval % 10); uval /= 10; } }
    if (neg) buf[pos++] = '-';
    /* reverse */
    for (int i = 0; i < pos / 2; ++i) {
        char tmp = buf[i]; buf[i] = buf[pos - 1 - i]; buf[pos - 1 - i] = tmp;
    }
    out(buf, (size_t)pos);
}

static void out_hex(unsigned long val) {
    char buf[16];
    int pos = 0;
    if (val == 0) { buf[pos++] = '0'; }
    else {
        while (val > 0) {
            unsigned d = (unsigned)(val & 0xF);
            buf[pos++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
            val >>= 4;
        }
    }
    for (int i = 0; i < pos / 2; ++i) {
        char tmp = buf[i]; buf[i] = buf[pos - 1 - i]; buf[pos - 1 - i] = tmp;
    }
    out(buf, (size_t)pos);
}

static long parse_long(const char *s) {
    long val = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); ++s; }
    return neg ? -val : val;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: printf format [args ...]\n", 31);
        return 1;
    }
    const char *fmt = argv[1];
    int argi = 2;

    while (*fmt) {
        if (*fmt == '\\') {
            ++fmt;
            switch (*fmt) {
            case 'n': out_char('\n'); break;
            case 't': out_char('\t'); break;
            case '\\': out_char('\\'); break;
            case '0': out_char('\0'); break;
            default:
                out_char('\\');
                if (*fmt) out_char(*fmt);
                break;
            }
            if (*fmt) ++fmt;
        } else if (*fmt == '%') {
            ++fmt;
            switch (*fmt) {
            case 's':
                if (argi < argc) out_str(argv[argi++]);
                break;
            case 'd':
                if (argi < argc) out_int(parse_long(argv[argi++]));
                break;
            case 'x':
                if (argi < argc) out_hex((unsigned long)parse_long(argv[argi++]));
                break;
            case '%':
                out_char('%');
                break;
            default:
                out_char('%');
                if (*fmt) out_char(*fmt);
                break;
            }
            if (*fmt) ++fmt;
        } else {
            out_char(*fmt++);
        }
    }
    return 0;
}
