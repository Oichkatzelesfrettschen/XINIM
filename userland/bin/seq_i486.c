#include <unistd.h>

static void write_long(int fd, long val) {
    char buf[24];
    int pos = 0;
    int neg = 0;
    unsigned long uval;
    if (val < 0) { neg = 1; uval = (unsigned long)(-val); }
    else uval = (unsigned long)val;
    if (uval == 0) buf[pos++] = '0';
    else while (uval > 0) { buf[pos++] = (char)('0' + uval % 10); uval /= 10; }
    if (neg) buf[pos++] = '-';
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = t;
    }
    write(fd, buf, pos);
}

static long parse_long(const char *s) {
    long v = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return neg ? -v : v;
}

int main(int argc, char **argv) {
    long first = 1, incr = 1, last = 1;
    if (argc == 2) { last = parse_long(argv[1]); }
    else if (argc == 3) { first = parse_long(argv[1]); last = parse_long(argv[2]); }
    else if (argc == 4) { first = parse_long(argv[1]); incr = parse_long(argv[2]); last = parse_long(argv[3]); }
    else { write(2, "usage: seq [first [incr]] last\n", 30); return 1; }
    if (incr == 0) return 1;
    for (long i = first; (incr > 0) ? (i <= last) : (i >= last); i += incr) {
        write_long(1, i);
        write(1, "\n", 1);
    }
    return 0;
}
