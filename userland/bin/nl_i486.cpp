// nl -- line numbering filter (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -b a (number all lines), -b t (number non-empty lines, default)

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

// Write a line number right-justified in 6 columns, followed by a tab.
void write_line_number(unsigned num) {
    char tmp[12];
    int n = 0;
    if (num == 0) { tmp[n++] = '0'; }
    else { while (num > 0) { tmp[n++] = static_cast<char>('0' + num % 10); num /= 10; } }

    char prefix[6] = {' ', ' ', ' ', ' ', ' ', ' '};
    int start = 6 - n;
    if (start < 0) start = 0;
    for (int j = 0; j < n && start + j < 6; ++j)
        prefix[start + j] = tmp[n - 1 - j];

    write_all(1, prefix, 6);
    write_all(1, "\t", 1);
}

enum class BodyType { ALL, NON_EMPTY };

int nl_fd(int fd, BodyType body, unsigned& line_num) {
    char buf[kBufSize];
    // We process character-by-character, tracking line starts
    bool at_line_start = true;
    bool blank_line = true; // track if current line is blank (for -b t)

    // We need to look ahead to know if a line is empty.
    // Strategy: buffer each line, then decide whether to number it.
    char line_buf[kBufSize];
    int line_len = 0;

    for (;;) {
        auto n = read(fd, buf, sizeof(buf));
        if (n == 0) {
            // Flush remaining line without newline
            if (line_len > 0) {
                bool is_empty = (line_len == 0);
                bool number_it = (body == BodyType::ALL) || (!is_empty);
                if (number_it) write_line_number(line_num++);
                else write_str(1, "      \t");
                write_all(1, line_buf, line_len);
            }
            return 0;
        }
        if (n < 0) return 1;

        for (int i = 0; i < static_cast<int>(n); ++i) {
            if (buf[i] == '\n') {
                bool is_empty = (line_len == 0);
                bool number_it = (body == BodyType::ALL) || !is_empty;
                if (number_it)
                    write_line_number(line_num++);
                else
                    write_str(1, "       ");
                write_all(1, line_buf, line_len);
                write_all(1, "\n", 1);
                line_len = 0;
            } else {
                if (line_len < kBufSize - 1)
                    line_buf[line_len++] = buf[i];
            }
        }
    }
}

bool str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a++ != *b++) return false; }
    return *a == *b;
}

} // namespace

int main(int argc, char** argv) {
    BodyType body = BodyType::NON_EMPTY;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'b') {
            const char* val = nullptr;
            if (argv[i][2] != '\0') {
                val = &argv[i][2];
            } else if (i + 1 < argc) {
                val = argv[++i];
            }
            if (val) {
                if (str_eq(val, "a")) body = BodyType::ALL;
                else if (str_eq(val, "t")) body = BodyType::NON_EMPTY;
            }
            first_arg = i + 1;
        } else if (argv[i][0] == '-') {
            first_arg = i + 1;
        } else {
            break;
        }
    }

    unsigned line_num = 1;

    if (first_arg >= argc) {
        return nl_fd(0, body, line_num);
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "nl: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (nl_fd(fd, body, line_num) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
