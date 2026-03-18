// printf -- format and print data (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports: %s, %d, %x, %o, %c, %%
// Escape sequences: \n, \t, \\, \0NNN

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

void write_char(int fd, char c) {
    write_all(fd, &c, 1);
}

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

long parse_int(const char* s) {
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

void write_decimal(int fd, long v) {
    if (v < 0) {
        write_char(fd, '-');
        // Handle minimum value edge case
        if (v == -2147483647L - 1) {
            write_str(fd, "2147483648");
            return;
        }
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

void write_hex(int fd, unsigned long v) {
    char tmp[16];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else {
        while (v > 0) {
            unsigned d = static_cast<unsigned>(v & 0xf);
            tmp[n++] = d < 10 ? static_cast<char>('0' + d) : static_cast<char>('a' + d - 10);
            v >>= 4;
        }
    }
    char out[16];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

void write_octal(int fd, unsigned long v) {
    char tmp[22];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else {
        while (v > 0) {
            tmp[n++] = static_cast<char>('0' + (v & 7));
            v >>= 3;
        }
    }
    char out[22];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

// Process escape sequences in format string, writing output.
// Returns the number of format-string chars consumed.
void process_escape(const char* fmt, int* pos) {
    ++(*pos); // skip backslash
    switch (fmt[*pos]) {
    case 'n':  write_char(1, '\n'); ++(*pos); break;
    case 't':  write_char(1, '\t'); ++(*pos); break;
    case '\\': write_char(1, '\\'); ++(*pos); break;
    case 'a':  write_char(1, '\a'); ++(*pos); break;
    case 'b':  write_char(1, '\b'); ++(*pos); break;
    case 'f':  write_char(1, '\f'); ++(*pos); break;
    case 'r':  write_char(1, '\r'); ++(*pos); break;
    case 'v':  write_char(1, '\v'); ++(*pos); break;
    case '0': {
        // Octal: \0NNN (up to 3 octal digits after the 0)
        ++(*pos); // skip '0'
        unsigned val = 0;
        for (int d = 0; d < 3 && fmt[*pos] >= '0' && fmt[*pos] <= '7'; ++d) {
            val = val * 8 + static_cast<unsigned>(fmt[*pos] - '0');
            ++(*pos);
        }
        char c = static_cast<char>(val & 0xff);
        write_all(1, &c, 1);
        break;
    }
    default:
        // Unknown escape: output backslash + char
        write_char(1, '\\');
        if (fmt[*pos] != '\0') {
            write_char(1, fmt[*pos]);
            ++(*pos);
        }
        break;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: printf format [arguments...]\n");
        return 1;
    }

    const char* fmt = argv[1];
    int argi = 2;  // index into argv for arguments

    int i = 0;
    while (fmt[i] != '\0') {
        if (fmt[i] == '\\') {
            process_escape(fmt, &i);
        } else if (fmt[i] == '%') {
            ++i;
            switch (fmt[i]) {
            case 's': {
                const char* s = (argi < argc) ? argv[argi++] : "";
                write_str(1, s);
                ++i;
                break;
            }
            case 'd': {
                long v = (argi < argc) ? parse_int(argv[argi++]) : 0;
                write_decimal(1, v);
                ++i;
                break;
            }
            case 'x': {
                unsigned long v = (argi < argc)
                    ? static_cast<unsigned long>(parse_int(argv[argi++])) : 0;
                write_hex(1, v);
                ++i;
                break;
            }
            case 'o': {
                unsigned long v = (argi < argc)
                    ? static_cast<unsigned long>(parse_int(argv[argi++])) : 0;
                write_octal(1, v);
                ++i;
                break;
            }
            case 'c': {
                char c = (argi < argc && argv[argi][0] != '\0')
                    ? argv[argi++][0] : '\0';
                if (c != '\0') write_char(1, c);
                ++i;
                break;
            }
            case '%':
                write_char(1, '%');
                ++i;
                break;
            case '\0':
                write_char(1, '%');
                break;
            default:
                write_char(1, '%');
                write_char(1, fmt[i]);
                ++i;
                break;
            }
        } else {
            write_char(1, fmt[i]);
            ++i;
        }
    }

    return 0;
}
