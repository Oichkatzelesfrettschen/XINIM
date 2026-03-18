// fold -- wrap each input line to fit in specified width (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -w WIDTH (default 80).

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

int fold_fd(int fd, int width) {
    char buf[kBufSize];
    int col = 0;

    for (;;) {
        auto n = read(fd, buf, sizeof(buf));
        if (n <= 0) return (n < 0) ? 1 : 0;

        for (int i = 0; i < static_cast<int>(n); ++i) {
            if (buf[i] == '\n') {
                write_all(1, "\n", 1);
                col = 0;
            } else if (buf[i] == '\t') {
                // Tab advances to next tab stop; fold if needed
                int next = col + (8 - col % 8);
                if (next > width) {
                    write_all(1, "\n", 1);
                    col = 0;
                }
                write_all(1, &buf[i], 1);
                col = col + (8 - col % 8);
            } else {
                if (col >= width) {
                    write_all(1, "\n", 1);
                    col = 0;
                }
                write_all(1, &buf[i], 1);
                ++col;
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    int width = 80;
    int first_file = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'w') {
            if (argv[i][2] != '\0') {
                width = parse_int(&argv[i][2]);
            } else if (i + 1 < argc) {
                width = parse_int(argv[++i]);
            }
            if (width <= 0) width = 80;
        } else if (argv[i][0] != '-' || (argv[i][0] == '-' && argv[i][1] == '\0')) {
            first_file = i;
            break;
        }
    }

    if (first_file == 0) {
        return fold_fd(0, width);
    }

    int status = 0;
    for (int i = first_file; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "fold: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (fold_fd(fd, width) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
