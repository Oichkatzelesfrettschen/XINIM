// expand -- convert tabs to spaces (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -t TABSTOP (default 8).

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;

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

int parse_int(const char* s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        ++s;
    }
    return val;
}

int expand_fd(int fd, int tabstop) {
    char buf[kBufSize];
    int col = 0;

    for (;;) {
        auto n = read(fd, buf, sizeof(buf));
        if (n <= 0) return (n < 0) ? 1 : 0;

        for (int i = 0; i < static_cast<int>(n); ++i) {
            if (buf[i] == '\t') {
                int spaces = tabstop - (col % tabstop);
                for (int s = 0; s < spaces; ++s)
                    write_all(1, " ", 1);
                col += spaces;
            } else if (buf[i] == '\n') {
                write_all(1, "\n", 1);
                col = 0;
            } else if (buf[i] == '\b') {
                write_all(1, &buf[i], 1);
                if (col > 0) --col;
            } else {
                write_all(1, &buf[i], 1);
                ++col;
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    int tabstop = 8;
    int first_file = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 't') {
            if (argv[i][2] != '\0') {
                tabstop = parse_int(&argv[i][2]);
            } else if (i + 1 < argc) {
                tabstop = parse_int(argv[++i]);
            }
            if (tabstop <= 0) tabstop = 8;
        } else if (argv[i][0] != '-' || (argv[i][0] == '-' && argv[i][1] == '\0')) {
            first_file = i;
            break;
        }
    }

    if (first_file == 0) {
        return expand_fd(0, tabstop);
    }

    int status = 0;
    for (int i = first_file; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "expand: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (expand_fd(fd, tabstop) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
