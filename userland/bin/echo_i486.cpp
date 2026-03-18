// echo -- write arguments to standard output (POSIX.1 + XSI)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -n (suppress trailing newline)
// XSI backslash escapes: \n \t \\ \a \b \c \f \r \v \0NNN

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

// Parse an octal sequence of up to 3 digits starting at s.
// Returns the character value and advances *pos past the digits.
char parse_octal(const char* s, int* pos) {
    unsigned val = 0;
    for (int d = 0; d < 3 && s[*pos] >= '0' && s[*pos] <= '7'; ++d) {
        val = val * 8 + static_cast<unsigned>(s[*pos] - '0');
        ++(*pos);
    }
    return static_cast<char>(val & 0xFF);
}

// Write a string with XSI backslash escape processing.
// Returns false if \c was encountered (suppress further output).
bool write_escaped(const char* s) {
    char buf[4096];
    int n = 0;

    for (int i = 0; s[i] != '\0'; ++i) {
        if (s[i] == '\\' && s[i + 1] != '\0') {
            switch (s[i + 1]) {
            case 'a':  buf[n++] = '\a'; ++i; break;
            case 'b':  buf[n++] = '\b'; ++i; break;
            case 'c':
                // Suppress trailing newline and stop output
                write_all(1, buf, n);
                return false;
            case 'f':  buf[n++] = '\f'; ++i; break;
            case 'n':  buf[n++] = '\n'; ++i; break;
            case 'r':  buf[n++] = '\r'; ++i; break;
            case 't':  buf[n++] = '\t'; ++i; break;
            case 'v':  buf[n++] = '\v'; ++i; break;
            case '\\': buf[n++] = '\\'; ++i; break;
            case '0': {
                ++i; // skip the backslash
                ++i; // skip the '0'
                int pos = i;
                buf[n++] = parse_octal(s, &pos);
                i = pos - 1; // loop increment will add 1
                break;
            }
            default:
                buf[n++] = s[i];
                break;
            }
        } else {
            buf[n++] = s[i];
        }
        if (n >= 4090) {
            write_all(1, buf, n);
            n = 0;
        }
    }
    if (n > 0) write_all(1, buf, n);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    bool no_newline = false;
    int first_arg = 1;

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0') {
        no_newline = true;
        first_arg = 2;
    }

    for (int i = first_arg; i < argc; ++i) {
        if (i > first_arg) write_all(1, " ", 1);
        if (!write_escaped(argv[i])) return 0;
    }

    if (!no_newline) write_all(1, "\n", 1);
    return 0;
}
