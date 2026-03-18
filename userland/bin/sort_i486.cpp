// sort -- sort lines of text (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -r (reverse), -n (numeric), -u (unique).
// Uses insertion sort (adequate for small files on i486).

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kMaxFile = 1048576; // 1 MiB

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

long parse_long(const char* s) {
    long val = 0;
    bool neg = false;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == '-') { neg = true; ++s; }
    else if (*s == '+') ++s;
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); ++s; }
    return neg ? -val : val;
}

int str_cmp(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

struct SortState {
    char** lines;
    int nlines;
    int cap;
    bool reverse;
    bool numeric;
    bool unique;
};

int compare(const char* a, const char* b, bool numeric) {
    if (numeric) {
        long la = parse_long(a);
        long lb = parse_long(b);
        if (la < lb) return -1;
        if (la > lb) return 1;
        return 0;
    }
    return str_cmp(a, b);
}

// Read all data from fd into a malloc'd buffer, split into lines.
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

bool add_line(SortState* st, char* line) {
    if (st->nlines >= st->cap) {
        int newcap = st->cap == 0 ? 256 : st->cap * 2;
        auto* nl = static_cast<char**>(realloc(st->lines,
                    static_cast<unsigned>(newcap) * sizeof(char*)));
        if (!nl) return false;
        st->lines = nl;
        st->cap = newcap;
    }
    st->lines[st->nlines++] = line;
    return true;
}

void split_lines(char* buf, int len, SortState* st) {
    if (len == 0) return;
    int start = 0;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            add_line(st, buf + start);
            start = i + 1;
        }
    }
    // Handle final line without trailing newline
    if (start < len) {
        buf[len] = '\0'; // safe -- we always have room for null
        add_line(st, buf + start);
    }
}

// Insertion sort -- O(n^2) but simple, stable, and adequate for small files.
void insertion_sort(SortState* st) {
    for (int i = 1; i < st->nlines; ++i) {
        char* key = st->lines[i];
        int j = i - 1;
        while (j >= 0) {
            int c = compare(st->lines[j], key, st->numeric);
            if (st->reverse) c = -c;
            if (c <= 0) break;
            st->lines[j + 1] = st->lines[j];
            --j;
        }
        st->lines[j + 1] = key;
    }
}

} // namespace

int main(int argc, char** argv) {
    SortState st{};
    int first_file = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 'r': st.reverse = true; break;
                case 'n': st.numeric = true; break;
                case 'u': st.unique = true; break;
                default: break;
                }
            }
            first_file = i + 1;
        } else {
            break;
        }
    }

    // Read input
    if (first_file >= argc) {
        int len = 0;
        char* buf = read_all(0, &len);
        if (!buf) { write_str(2, "sort: read error\n"); return 1; }
        split_lines(buf, len, &st);
    } else {
        for (int i = first_file; i < argc; ++i) {
            int fd;
            if (argv[i][0] == '-' && argv[i][1] == '\0') {
                fd = 0;
            } else {
                fd = open(argv[i], O_RDONLY, 0);
                if (fd < 0) {
                    write_str(2, "sort: ");
                    write_str(2, argv[i]);
                    write_str(2, ": No such file or directory\n");
                    continue;
                }
            }
            int len = 0;
            char* buf = read_all(fd, &len);
            if (fd != 0) close(fd);
            if (!buf) continue;
            split_lines(buf, len, &st);
            // Note: buf is not freed because lines point into it.
        }
    }

    insertion_sort(&st);

    // Output
    const char* prev = nullptr;
    for (int i = 0; i < st.nlines; ++i) {
        if (st.unique && prev != nullptr && str_cmp(prev, st.lines[i]) == 0)
            continue;
        prev = st.lines[i];
        int slen = str_len(st.lines[i]);
        write_all(1, st.lines[i], slen);
        write_all(1, "\n", 1);
    }

    free(st.lines);
    return 0;
}
