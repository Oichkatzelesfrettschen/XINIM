// cmp -- compare two files byte by byte (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Reports first difference with byte offset and line number.
// Supports -s (silent).

#include <fcntl.h>
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

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

void write_ulong(int fd, unsigned long v) {
    char tmp[20];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[20];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

} // namespace

int main(int argc, char** argv) {
    bool silent = false;
    int first = 1;

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 's' && argv[1][2] == '\0') {
        silent = true;
        first = 2;
    }

    if (argc - first < 2) {
        write_str(2, "usage: cmp [-s] file1 file2\n");
        return 2;
    }

    const char* path1 = argv[first];
    const char* path2 = argv[first + 1];

    int fd1 = open(path1, O_RDONLY, 0);
    if (fd1 < 0) {
        if (!silent) { write_str(2, "cmp: "); write_str(2, path1); write_str(2, ": cannot open\n"); }
        return 2;
    }
    int fd2 = open(path2, O_RDONLY, 0);
    if (fd2 < 0) {
        close(fd1);
        if (!silent) { write_str(2, "cmp: "); write_str(2, path2); write_str(2, ": cannot open\n"); }
        return 2;
    }

    unsigned long byte_num = 1;
    unsigned long line_num = 1;
    char c1, c2;

    for (;;) {
        auto r1 = read(fd1, &c1, 1);
        auto r2 = read(fd2, &c2, 1);

        if (r1 <= 0 && r2 <= 0) { close(fd1); close(fd2); return 0; }

        if (r1 <= 0) {
            if (!silent) { write_str(1, "cmp: EOF on "); write_str(1, path1); write_str(1, "\n"); }
            close(fd1); close(fd2);
            return 1;
        }
        if (r2 <= 0) {
            if (!silent) { write_str(1, "cmp: EOF on "); write_str(1, path2); write_str(1, "\n"); }
            close(fd1); close(fd2);
            return 1;
        }

        if (c1 != c2) {
            if (!silent) {
                write_str(1, path1);
                write_str(1, " ");
                write_str(1, path2);
                write_str(1, " differ: byte ");
                write_ulong(1, byte_num);
                write_str(1, ", line ");
                write_ulong(1, line_num);
                write_str(1, "\n");
            }
            close(fd1);
            close(fd2);
            return 1;
        }

        if (c1 == '\n') ++line_num;
        ++byte_num;
    }
}
