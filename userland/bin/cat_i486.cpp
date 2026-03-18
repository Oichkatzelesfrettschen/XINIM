// cat -- concatenate and print files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -u (unbuffered, default for us), -n (number lines)

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

void write_uint(int fd, unsigned v) {
    char tmp[12];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[12];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

int cat_fd(int fd, bool number_lines, unsigned& line_num) {
    char buf[kBufSize];
    bool at_line_start = true;
    for (;;) {
        auto n = read(fd, buf, sizeof(buf));
        if (n == 0) return 0;
        if (n < 0) return 1;
        if (!number_lines) {
            write_all(1, buf, static_cast<int>(n));
        } else {
            for (int i = 0; i < static_cast<int>(n); ++i) {
                if (at_line_start) {
                    // Right-justify line number in 6 columns
                    char prefix[10] = "      ";
                    char num[8];
                    int nlen = 0;
                    unsigned v = line_num++;
                    if (v == 0) { num[nlen++] = '0'; }
                    else { while (v > 0) { num[nlen++] = static_cast<char>('0' + v % 10); v /= 10; } }
                    int start = 6 - nlen;
                    if (start < 0) start = 0;
                    for (int j = 0; j < nlen && start + j < 6; ++j)
                        prefix[start + j] = num[nlen - 1 - j];
                    write_all(1, prefix, 6);
                    write_all(1, "\t", 1);
                    at_line_start = false;
                }
                write_all(1, &buf[i], 1);
                if (buf[i] == '\n') at_line_start = true;
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    bool number_lines = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 'n': number_lines = true; break;
                case 'u': break; // always unbuffered
                default: break;
                }
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    unsigned line_num = 1;
    if (first_arg >= argc) {
        return cat_fd(0, number_lines, line_num);
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            if (cat_fd(0, number_lines, line_num) != 0) status = 1;
            continue;
        }
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "cat: ");
            write_str(2, argv[i]);
            write_str(2, ": No such file or directory\n");
            status = 1;
            continue;
        }
        if (cat_fd(fd, number_lines, line_num) != 0) status = 1;
        close(fd);
    }
    return status;
}
