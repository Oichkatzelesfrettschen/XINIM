// tail -- output the last part of a file (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -n COUNT (lines, default 10)

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kMaxFile = 1048576; // 1 MiB max buffer

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

// Read entire fd into a malloc'd buffer. Returns size via *out_len.
char* read_all(int fd, int* out_len) {
    int cap = kBufSize;
    int len = 0;
    auto* buf = static_cast<char*>(malloc(static_cast<unsigned>(cap)));
    if (!buf) return nullptr;

    for (;;) {
        if (len + kBufSize > cap) {
            cap *= 2;
            if (cap > kMaxFile) cap = kMaxFile;
            auto* nb = static_cast<char*>(realloc(buf, static_cast<unsigned>(cap)));
            if (!nb) { free(buf); return nullptr; }
            buf = nb;
        }
        auto n = read(fd, buf + len, static_cast<unsigned>(cap - len));
        if (n == 0) break;
        if (n < 0) { free(buf); return nullptr; }
        len += static_cast<int>(n);
    }
    *out_len = len;
    return buf;
}

int tail_fd(int fd, int count) {
    int len = 0;
    char* buf = read_all(fd, &len);
    if (!buf) {
        write_str(2, "tail: read error\n");
        return 1;
    }
    if (len == 0) { free(buf); return 0; }

    // Walk backward from end counting newlines
    int lines_found = 0;
    int start = len;

    // If file ends with newline, skip it for counting purposes
    if (len > 0 && buf[len - 1] == '\n') --start;

    for (int i = start - 1; i >= 0; --i) {
        if (buf[i] == '\n') {
            ++lines_found;
            if (lines_found >= count) {
                start = i + 1;
                break;
            }
        }
    }
    if (lines_found < count) start = 0;

    write_all(1, buf + start, len - start);
    free(buf);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    int count = 10;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (argv[i][1] == 'n') {
                if (argv[i][2] != '\0')
                    count = parse_int(&argv[i][2]);
                else if (i + 1 < argc)
                    count = parse_int(argv[++i]);
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    if (first_arg >= argc) {
        return tail_fd(0, count);
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "tail: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (tail_fd(fd, count) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
