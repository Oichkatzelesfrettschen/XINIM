// wc -- word, line, and byte count (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -l (lines), -w (words), -c (bytes). Default: all three.

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

// Write an unsigned long right-justified in a field of 'width' chars.
void write_field(int fd, unsigned long val, int width) {
    char tmp[20];
    int n = 0;
    if (val == 0) { tmp[n++] = '0'; }
    else { while (val > 0) { tmp[n++] = static_cast<char>('0' + val % 10); val /= 10; } }

    // Pad with spaces
    for (int i = 0; i < width - n; ++i)
        write_all(fd, " ", 1);

    // Write digits in correct order
    for (int i = n - 1; i >= 0; --i)
        write_all(fd, &tmp[i], 1);
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

struct Counts {
    unsigned long lines;
    unsigned long words;
    unsigned long bytes;
};

int count_fd(int fd, Counts* c) {
    char buf[kBufSize];
    c->lines = 0;
    c->words = 0;
    c->bytes = 0;
    bool in_word = false;

    for (;;) {
        auto n = read(fd, buf, sizeof(buf));
        if (n == 0) return 0;
        if (n < 0) return 1;
        c->bytes += static_cast<unsigned long>(n);
        for (int i = 0; i < static_cast<int>(n); ++i) {
            if (buf[i] == '\n') ++(c->lines);
            if (is_space(buf[i])) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                ++(c->words);
            }
        }
    }
}

void print_counts(const Counts& c, bool show_lines, bool show_words, bool show_bytes,
                   const char* name, bool first_field) {
    int width = 7;
    if (show_lines) {
        if (!first_field) write_all(1, " ", 1);
        write_field(1, c.lines, width);
        first_field = false;
    }
    if (show_words) {
        if (!first_field) write_all(1, " ", 1);
        write_field(1, c.words, width);
        first_field = false;
    }
    if (show_bytes) {
        if (!first_field) write_all(1, " ", 1);
        write_field(1, c.bytes, width);
    }
    if (name) {
        write_all(1, " ", 1);
        write_str(1, name);
    }
    write_all(1, "\n", 1);
}

} // namespace

int main(int argc, char** argv) {
    bool show_lines = false;
    bool show_words = false;
    bool show_bytes = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 'l': show_lines = true; break;
                case 'w': show_words = true; break;
                case 'c': show_bytes = true; break;
                default: break;
                }
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    // Default: show all three
    if (!show_lines && !show_words && !show_bytes) {
        show_lines = true;
        show_words = true;
        show_bytes = true;
    }

    int file_count = argc - first_arg;

    if (file_count <= 0) {
        Counts c{};
        if (count_fd(0, &c) != 0) return 1;
        print_counts(c, show_lines, show_words, show_bytes, nullptr, true);
        return 0;
    }

    int status = 0;
    Counts total{0, 0, 0};

    for (int i = first_arg; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "wc: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }

        Counts c{};
        if (count_fd(fd, &c) != 0) {
            status = 1;
        } else {
            print_counts(c, show_lines, show_words, show_bytes, argv[i], true);
            total.lines += c.lines;
            total.words += c.words;
            total.bytes += c.bytes;
        }
        if (fd != 0) close(fd);
    }

    if (file_count > 1) {
        print_counts(total, show_lines, show_words, show_bytes, "total", true);
    }

    return status;
}
