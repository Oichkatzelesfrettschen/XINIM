// uniq -- report or filter out repeated lines (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -c (count), -d (only duplicates), -u (only unique).

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kLineMax = 4096;

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

void write_ulong_padded(int fd, unsigned long val, int width) {
    char tmp[20];
    int n = 0;
    if (val == 0) { tmp[n++] = '0'; }
    else { while (val > 0) { tmp[n++] = static_cast<char>('0' + val % 10); val /= 10; } }
    // Right-justify in field
    for (int i = 0; i < width - n; ++i)
        write_all(fd, " ", 1);
    for (int i = n - 1; i >= 0; --i)
        write_all(fd, &tmp[i], 1);
}

int str_cmp(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

struct Opts {
    bool show_count;
    bool only_dupes;
    bool only_unique;
};

void emit(const char* line, unsigned long count, const Opts& opts) {
    bool is_dup = count > 1;
    if (opts.only_dupes && !is_dup) return;
    if (opts.only_unique && is_dup) return;
    if (opts.show_count) {
        write_ulong_padded(1, count, 7);
        write_all(1, " ", 1);
    }
    write_all(1, line, str_len(line));
    write_all(1, "\n", 1);
}

} // namespace

int main(int argc, char** argv) {
    Opts opts{};
    int first_file = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 'c': opts.show_count = true; break;
                case 'd': opts.only_dupes = true; break;
                case 'u': opts.only_unique = true; break;
                default: break;
                }
            }
            first_file = i + 1;
        } else {
            break;
        }
    }

    int fd = 0;
    if (first_file < argc) {
        if (argv[first_file][0] == '-' && argv[first_file][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[first_file], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "uniq: ");
                write_str(2, argv[first_file]);
                write_str(2, ": No such file or directory\n");
                return 1;
            }
        }
    }

    char prev[kLineMax];
    char line[kLineMax];
    int line_len = 0;
    bool prev_valid = false;
    unsigned long count = 0;

    char buf[kBufSize];
    int buf_pos = 0;
    int buf_end = 0;

    for (;;) {
        if (buf_pos >= buf_end) {
            auto n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            buf_pos = 0;
            buf_end = static_cast<int>(n);
        }

        char ch = buf[buf_pos++];
        if (ch == '\n' || line_len >= kLineMax - 1) {
            line[line_len] = '\0';

            if (prev_valid && str_cmp(line, prev) == 0) {
                ++count;
            } else {
                if (prev_valid) emit(prev, count, opts);
                memcpy(prev, line, static_cast<unsigned>(line_len + 1));
                count = 1;
                prev_valid = true;
            }
            line_len = 0;
        } else {
            line[line_len++] = ch;
        }
    }

    // Handle final line without trailing newline
    if (line_len > 0) {
        line[line_len] = '\0';
        if (prev_valid && str_cmp(line, prev) == 0) {
            ++count;
        } else {
            if (prev_valid) emit(prev, count, opts);
            memcpy(prev, line, static_cast<unsigned>(line_len + 1));
            count = 1;
            prev_valid = true;
        }
    }

    if (prev_valid) emit(prev, count, opts);

    if (fd != 0) close(fd);
    return 0;
}
