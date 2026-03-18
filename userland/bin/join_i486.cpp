// join -- relational join of two sorted files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Joins on a common field (default: field 1). Delimiter is whitespace (tab/space).
// Both files must be sorted on the join field.

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 32768;
constexpr int kMaxLines = 4096;
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

int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

int str_cmp(const char* a, int alen, const char* b, int blen) {
    int minlen = alen < blen ? alen : blen;
    for (int i = 0; i < minlen; ++i) {
        if (static_cast<unsigned char>(a[i]) != static_cast<unsigned char>(b[i]))
            return static_cast<unsigned char>(a[i]) - static_cast<unsigned char>(b[i]);
    }
    return alen - blen;
}

bool is_delim(char c) {
    return c == ' ' || c == '\t';
}

// Extract field N (1-based) from line. Sets *fstart and *flen.
void get_field(const char* line, int len, int field_num, int* fstart, int* flen) {
    int f = 1;
    int i = 0;

    // Skip leading whitespace
    while (i < len && is_delim(line[i])) ++i;

    while (f < field_num && i < len) {
        // Skip current field content
        while (i < len && !is_delim(line[i])) ++i;
        // Skip delimiter(s)
        while (i < len && is_delim(line[i])) ++i;
        ++f;
    }

    *fstart = i;
    int end = i;
    while (end < len && !is_delim(line[end])) ++end;
    *flen = end - i;
}

struct FileData {
    char* buf;
    char* lines[kMaxLines];
    int nlines;
};

int read_file(int fd, FileData* f) {
    f->buf = static_cast<char*>(malloc(kBufSize));
    if (!f->buf) return -1;
    int total = 0;
    for (;;) {
        auto n = read(fd, f->buf + total, static_cast<unsigned>(kBufSize - 1 - total));
        if (n <= 0) break;
        total += static_cast<int>(n);
        if (total >= kBufSize - 1) break;
    }
    f->buf[total] = '\0';
    f->nlines = 0;

    char* p = f->buf;
    while (*p != '\0' && f->nlines < kMaxLines) {
        f->lines[f->nlines++] = p;
        char* nl = p;
        while (*nl != '\0' && *nl != '\n') ++nl;
        if (*nl == '\n') { *nl = '\0'; p = nl + 1; }
        else break;
    }
    return 0;
}

// Output a joined line: join field, then remaining fields from file1, then from file2.
void output_join(const char* line1, int len1, const char* line2, int len2,
                 int jfield, char sep) {
    int js, jl;
    get_field(line1, len1, jfield, &js, &jl);

    // Print join field
    write_all(1, line1 + js, jl);

    // Print non-join fields from line1
    int f = 1;
    int i = 0;
    while (i < len1 && is_delim(line1[i])) ++i;
    while (i < len1) {
        int fstart = i;
        while (i < len1 && !is_delim(line1[i])) ++i;
        if (f != jfield) {
            write_all(1, &sep, 1);
            write_all(1, line1 + fstart, i - fstart);
        }
        while (i < len1 && is_delim(line1[i])) ++i;
        ++f;
    }

    // Print non-join fields from line2
    f = 1;
    i = 0;
    while (i < len2 && is_delim(line2[i])) ++i;
    while (i < len2) {
        int fstart = i;
        while (i < len2 && !is_delim(line2[i])) ++i;
        if (f != jfield) {
            write_all(1, &sep, 1);
            write_all(1, line2 + fstart, i - fstart);
        }
        while (i < len2 && is_delim(line2[i])) ++i;
        ++f;
    }

    write_all(1, "\n", 1);
}

} // namespace

int main(int argc, char** argv) {
    int jfield = 1;
    char sep = ' ';
    int argi = 1;

    // Parse options
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        if (argv[argi][1] == 'j' || argv[argi][1] == '1' || argv[argi][1] == '2') {
            // -j FIELD, -1 FIELD, -2 FIELD (simplified: use same field for both)
            const char* val = nullptr;
            if (argv[argi][2] != '\0') {
                val = &argv[argi][2];
            } else if (argi + 1 < argc) {
                val = argv[++argi];
            }
            if (val) {
                jfield = 0;
                while (*val >= '0' && *val <= '9') {
                    jfield = jfield * 10 + (*val - '0');
                    ++val;
                }
            }
        } else if (argv[argi][1] == 't') {
            if (argv[argi][2] != '\0') {
                sep = argv[argi][2];
            } else if (argi + 1 < argc) {
                sep = argv[++argi][0];
            }
        }
        ++argi;
    }

    if (argc - argi < 2) {
        write_str(2, "usage: join [-j FIELD] [-t CHAR] file1 file2\n");
        return 1;
    }

    int fd1 = open(argv[argi], O_RDONLY, 0);
    if (fd1 < 0) {
        write_str(2, "join: ");
        write_str(2, argv[argi]);
        write_str(2, ": No such file or directory\n");
        return 1;
    }
    int fd2 = open(argv[argi + 1], O_RDONLY, 0);
    if (fd2 < 0) {
        write_str(2, "join: ");
        write_str(2, argv[argi + 1]);
        write_str(2, ": No such file or directory\n");
        close(fd1);
        return 1;
    }

    FileData f1{}, f2{};
    read_file(fd1, &f1); close(fd1);
    read_file(fd2, &f2); close(fd2);

    // Merge-join on the join field
    int i = 0, j = 0;
    while (i < f1.nlines && j < f2.nlines) {
        int len1 = str_len(f1.lines[i]);
        int len2 = str_len(f2.lines[j]);

        int js1, jl1, js2, jl2;
        get_field(f1.lines[i], len1, jfield, &js1, &jl1);
        get_field(f2.lines[j], len2, jfield, &js2, &jl2);

        int cmp = str_cmp(f1.lines[i] + js1, jl1, f2.lines[j] + js2, jl2);

        if (cmp == 0) {
            // For each matching line in file2, output with current file1 line
            int jj = j;
            while (jj < f2.nlines) {
                int l2 = str_len(f2.lines[jj]);
                int s2, sl2;
                get_field(f2.lines[jj], l2, jfield, &s2, &sl2);
                if (str_cmp(f1.lines[i] + js1, jl1, f2.lines[jj] + s2, sl2) != 0)
                    break;
                output_join(f1.lines[i], len1, f2.lines[jj], l2, jfield, sep);
                ++jj;
            }
            ++i;
        } else if (cmp < 0) {
            ++i;
        } else {
            ++j;
        }
    }

    free(f1.buf);
    free(f2.buf);
    return 0;
}
