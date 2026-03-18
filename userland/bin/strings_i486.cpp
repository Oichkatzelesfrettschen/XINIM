// strings -- print printable character sequences from files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Default minimum length: 4. Supports -n MIN_LEN.

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kStringMax = 4096;

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

bool is_printable(unsigned char c) {
    return c >= 0x20 && c < 0x7f;
}

void strings_fd(int fd, int min_len) {
    char strbuf[kStringMax];
    int slen = 0;
    unsigned char buf[kBufSize];

    for (;;) {
        auto nr = read(fd, buf, sizeof(buf));
        if (nr <= 0) break;
        for (int i = 0; i < static_cast<int>(nr); ++i) {
            if (is_printable(buf[i]) || buf[i] == '\t') {
                if (slen < kStringMax - 1)
                    strbuf[slen++] = static_cast<char>(buf[i]);
            } else {
                if (slen >= min_len) {
                    write_all(1, strbuf, slen);
                    write_all(1, "\n", 1);
                }
                slen = 0;
            }
        }
    }

    // Flush trailing string
    if (slen >= min_len) {
        write_all(1, strbuf, slen);
        write_all(1, "\n", 1);
    }
}

} // namespace

int main(int argc, char** argv) {
    int min_len = 4;
    int first = 1;

    // Parse -n option
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0') {
        min_len = 0;
        for (int j = 0; argv[2][j] != '\0'; ++j)
            min_len = min_len * 10 + (argv[2][j] - '0');
        if (min_len < 1) min_len = 1;
        first = 3;
    }
    // Also handle -nN form
    else if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] != '\0') {
        min_len = 0;
        for (int j = 2; argv[1][j] != '\0'; ++j)
            min_len = min_len * 10 + (argv[1][j] - '0');
        if (min_len < 1) min_len = 1;
        first = 2;
    }
    // Also handle -N form (just a number)
    else if (argc > 1 && argv[1][0] == '-' && argv[1][1] >= '0' && argv[1][1] <= '9') {
        min_len = 0;
        for (int j = 1; argv[1][j] != '\0'; ++j)
            min_len = min_len * 10 + (argv[1][j] - '0');
        if (min_len < 1) min_len = 1;
        first = 2;
    }

    if (first >= argc) {
        // Read from stdin
        strings_fd(0, min_len);
        return 0;
    }

    for (int i = first; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "strings: ");
            write_str(2, argv[i]);
            write_str(2, ": cannot open\n");
            continue;
        }
        strings_fd(fd, min_len);
        close(fd);
    }

    return 0;
}
