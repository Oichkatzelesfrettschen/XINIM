// head -- output the first part of files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -n COUNT (lines), -c COUNT (bytes)

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
    for (int i = 0; s[i] >= '0' && s[i] <= '9'; ++i)
        val = val * 10 + (s[i] - '0');
    return val;
}

int head_lines(int fd, int count) {
    char buf[kBufSize];
    int lines = 0;
    for (;;) {
        auto n = read(fd, buf, sizeof(buf));
        if (n <= 0) return (n < 0) ? 1 : 0;
        for (int i = 0; i < static_cast<int>(n); ++i) {
            write_all(1, &buf[i], 1);
            if (buf[i] == '\n') {
                ++lines;
                if (lines >= count) return 0;
            }
        }
    }
}

int head_bytes(int fd, int count) {
    char buf[kBufSize];
    int remaining = count;
    while (remaining > 0) {
        int to_read = remaining < kBufSize ? remaining : kBufSize;
        auto n = read(fd, buf, static_cast<unsigned>(to_read));
        if (n <= 0) return (n < 0) ? 1 : 0;
        write_all(1, buf, static_cast<int>(n));
        remaining -= static_cast<int>(n);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    int line_count = 10;
    int byte_count = 0;
    bool use_bytes = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (argv[i][1] == 'n') {
                if (argv[i][2] != '\0') {
                    line_count = parse_int(&argv[i][2]);
                } else if (i + 1 < argc) {
                    line_count = parse_int(argv[++i]);
                }
                use_bytes = false;
            } else if (argv[i][1] == 'c') {
                if (argv[i][2] != '\0') {
                    byte_count = parse_int(&argv[i][2]);
                } else if (i + 1 < argc) {
                    byte_count = parse_int(argv[++i]);
                }
                use_bytes = true;
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    int file_count = argc - first_arg;

    if (file_count <= 0) {
        // Read from stdin
        if (use_bytes)
            return head_bytes(0, byte_count);
        else
            return head_lines(0, line_count);
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        if (file_count > 1) {
            if (i > first_arg) write_all(1, "\n", 1);
            write_str(1, "==> ");
            write_str(1, argv[i]);
            write_str(1, " <==\n");
        }

        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "head: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }

        int r;
        if (use_bytes)
            r = head_bytes(fd, byte_count);
        else
            r = head_lines(fd, line_count);
        if (r != 0) status = 1;

        if (fd != 0) close(fd);
    }
    return status;
}
