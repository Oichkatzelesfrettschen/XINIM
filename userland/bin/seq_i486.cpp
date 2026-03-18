// seq -- print a sequence of numbers (XSI)
// Cleanroom C++23 implementation.
// Usage: seq LAST, seq FIRST LAST, seq FIRST INCREMENT LAST

#include <unistd.h>

namespace {

void write_all(int fd, const char* buf, int len) {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w;
        len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

void write_long(int fd, long v) {
    if (v < 0) {
        write_all(fd, "-", 1);
        if (v == -2147483647L - 1) { write_str(fd, "2147483648"); return; }
        v = -v;
    }
    char tmp[20];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[20];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

long parse_long(const char* s) {
    long v = 0;
    bool neg = false;
    if (*s == '-') { neg = true; ++s; }
    else if (*s == '+') { ++s; }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        ++s;
    }
    return neg ? -v : v;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: seq [FIRST [INCREMENT]] LAST\n");
        return 1;
    }

    long first = 1, incr = 1, last = 1;

    if (argc == 2) {
        last = parse_long(argv[1]);
    } else if (argc == 3) {
        first = parse_long(argv[1]);
        last = parse_long(argv[2]);
    } else {
        first = parse_long(argv[1]);
        incr = parse_long(argv[2]);
        last = parse_long(argv[3]);
    }

    if (incr == 0) {
        write_str(2, "seq: zero increment\n");
        return 1;
    }

    for (long v = first; (incr > 0) ? (v <= last) : (v >= last); v += incr) {
        write_long(1, v);
        write_all(1, "\n", 1);
    }

    return 0;
}
