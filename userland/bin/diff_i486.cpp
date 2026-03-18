// diff -- compare files line by line (simplified)
// Cleanroom C++23 implementation.
// Sequential comparison: lines only in file1 marked with <, only in file2 with >.
// Not a full LCS algorithm -- simple line-by-line comparison.

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kMaxLines = 8192;
constexpr int kMaxLineLen = 1024;
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

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

bool lines_equal(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

// Slurp file into line array. Returns number of lines.
// Each line is stored in storage[line_num] with null terminator.
static char file1_storage[kMaxLines][kMaxLineLen];
static char file2_storage[kMaxLines][kMaxLineLen];

int read_lines(const char* path, char storage[][kMaxLineLen]) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_str(2, "diff: ");
        write_str(2, path);
        write_str(2, ": cannot open\n");
        return -1;
    }

    int line_count = 0;
    int col = 0;
    char buf[kBufSize];

    for (;;) {
        auto nr = read(fd, buf, sizeof(buf));
        if (nr <= 0) break;
        for (int i = 0; i < static_cast<int>(nr); ++i) {
            if (buf[i] == '\n') {
                if (line_count < kMaxLines) {
                    storage[line_count][col] = '\0';
                    ++line_count;
                }
                col = 0;
            } else {
                if (line_count < kMaxLines && col < kMaxLineLen - 1) {
                    storage[line_count][col++] = buf[i];
                }
            }
        }
    }
    // Handle last line without trailing newline
    if (col > 0 && line_count < kMaxLines) {
        storage[line_count][col] = '\0';
        ++line_count;
    }

    close(fd);
    return line_count;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        write_str(2, "usage: diff file1 file2\n");
        return 2;
    }

    int n1 = read_lines(argv[1], file1_storage);
    int n2 = read_lines(argv[2], file2_storage);
    if (n1 < 0 || n2 < 0) return 2;

    bool differs = false;
    int i1 = 0, i2 = 0;

    while (i1 < n1 && i2 < n2) {
        if (lines_equal(file1_storage[i1], file2_storage[i2])) {
            ++i1;
            ++i2;
            continue;
        }

        differs = true;

        // Look ahead in file2 to see if file1's current line appears soon
        bool found_in_f2 = false;
        for (int look = i2 + 1; look < n2 && look <= i2 + 16; ++look) {
            if (lines_equal(file1_storage[i1], file2_storage[look])) {
                // Lines i2..look-1 are added in file2
                write_uint(1, static_cast<unsigned>(i1 + 1));
                write_str(1, "a");
                write_uint(1, static_cast<unsigned>(i2 + 1));
                if (look > i2 + 1) {
                    write_str(1, ",");
                    write_uint(1, static_cast<unsigned>(look));
                }
                write_str(1, "\n");
                for (int k = i2; k < look; ++k) {
                    write_str(1, "> ");
                    write_str(1, file2_storage[k]);
                    write_str(1, "\n");
                }
                i2 = look;
                found_in_f2 = true;
                break;
            }
        }

        if (found_in_f2) continue;

        // Look ahead in file1 to see if file2's current line appears soon
        bool found_in_f1 = false;
        for (int look = i1 + 1; look < n1 && look <= i1 + 16; ++look) {
            if (lines_equal(file2_storage[i2], file1_storage[look])) {
                // Lines i1..look-1 are deleted from file1
                write_uint(1, static_cast<unsigned>(i1 + 1));
                if (look > i1 + 1) {
                    write_str(1, ",");
                    write_uint(1, static_cast<unsigned>(look));
                }
                write_str(1, "d");
                write_uint(1, static_cast<unsigned>(i2));
                write_str(1, "\n");
                for (int k = i1; k < look; ++k) {
                    write_str(1, "< ");
                    write_str(1, file1_storage[k]);
                    write_str(1, "\n");
                }
                i1 = look;
                found_in_f1 = true;
                break;
            }
        }

        if (found_in_f1) continue;

        // No nearby match -- treat as a change
        write_uint(1, static_cast<unsigned>(i1 + 1));
        write_str(1, "c");
        write_uint(1, static_cast<unsigned>(i2 + 1));
        write_str(1, "\n");
        write_str(1, "< ");
        write_str(1, file1_storage[i1]);
        write_str(1, "\n");
        write_str(1, "---\n");
        write_str(1, "> ");
        write_str(1, file2_storage[i2]);
        write_str(1, "\n");
        ++i1;
        ++i2;
    }

    // Remaining lines in file1
    while (i1 < n1) {
        differs = true;
        write_uint(1, static_cast<unsigned>(i1 + 1));
        write_str(1, "d");
        write_uint(1, static_cast<unsigned>(i2));
        write_str(1, "\n");
        write_str(1, "< ");
        write_str(1, file1_storage[i1]);
        write_str(1, "\n");
        ++i1;
    }

    // Remaining lines in file2
    while (i2 < n2) {
        differs = true;
        write_uint(1, static_cast<unsigned>(i1));
        write_str(1, "a");
        write_uint(1, static_cast<unsigned>(i2 + 1));
        write_str(1, "\n");
        write_str(1, "> ");
        write_str(1, file2_storage[i2]);
        write_str(1, "\n");
        ++i2;
    }

    return differs ? 1 : 0;
}
