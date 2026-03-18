// grep -- search files for patterns (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -i (case-insensitive), -v (invert), -c (count), -n (line numbers),
//          -l (files-with-matches), -q (quiet).
// Supports fixed strings and basic . (any char) and * (zero or more of prev).

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

void write_ulong(int fd, unsigned long val) {
    char tmp[20];
    int n = 0;
    if (val == 0) { tmp[n++] = '0'; }
    else { while (val > 0) { tmp[n++] = static_cast<char>('0' + val % 10); val /= 10; } }
    for (int i = n - 1; i >= 0; --i)
        write_all(fd, &tmp[i], 1);
}

char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c + 32);
    return c;
}

// Basic pattern matching supporting . (any char) and * (zero or more of prev).
// Recursive backtracking matcher operating on null-terminated strings.
bool match_here(const char* pat, const char* text, bool icase);

bool match_star(char prev_c, const char* pat, const char* text, bool icase) {
    // prev_c* -- match zero or more of prev_c
    for (const char* t = text; ; ++t) {
        if (match_here(pat, t, icase)) return true;
        if (*t == '\0') return false;
        if (prev_c != '.') {
            char tc = icase ? to_lower(*t) : *t;
            char pc = icase ? to_lower(prev_c) : prev_c;
            if (tc != pc) return false;
        }
    }
}

bool match_here(const char* pat, const char* text, bool icase) {
    if (pat[0] == '\0') return true;
    if (pat[1] == '*') return match_star(pat[0], pat + 2, text, icase);
    if (*text == '\0') return false;
    if (pat[0] == '.') return match_here(pat + 1, text + 1, icase);
    char pc = icase ? to_lower(pat[0]) : pat[0];
    char tc = icase ? to_lower(*text) : *text;
    if (pc == tc) return match_here(pat + 1, text + 1, icase);
    return false;
}

// Search for pattern anywhere in text (unanchored match).
bool match_line(const char* pat, const char* text, bool icase) {
    // If pattern starts with ^, anchor at beginning
    if (pat[0] == '^') return match_here(pat + 1, text, icase);
    // Try matching at every position
    for (const char* t = text; ; ++t) {
        if (match_here(pat, t, icase)) return true;
        if (*t == '\0') return false;
    }
}

struct Opts {
    bool invert;
    bool icase;
    bool count;
    bool number;
    bool files_only;
    bool quiet;
};

int grep_fd(int fd, const char* pattern, const char* fname, bool show_name, const Opts& opts) {
    char buf[kBufSize];
    char line[kLineMax];
    int line_len = 0;
    int buf_pos = 0;
    int buf_end = 0;
    unsigned long match_count = 0;
    unsigned long line_no = 0;
    int status = 1;

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
            ++line_no;

            bool matched = match_line(pattern, line, opts.icase);
            if (opts.invert) matched = !matched;

            if (matched) {
                ++match_count;
                status = 0;
                if (opts.quiet) return 0;
                if (opts.files_only) {
                    write_str(1, fname);
                    write_all(1, "\n", 1);
                    return 0;
                }
                if (!opts.count) {
                    if (show_name) { write_str(1, fname); write_all(1, ":", 1); }
                    if (opts.number) { write_ulong(1, line_no); write_all(1, ":", 1); }
                    write_all(1, line, line_len);
                    write_all(1, "\n", 1);
                }
            }
            line_len = 0;
        } else {
            line[line_len++] = ch;
        }
    }

    // Handle final line without trailing newline
    if (line_len > 0) {
        line[line_len] = '\0';
        ++line_no;

        bool matched = match_line(pattern, line, opts.icase);
        if (opts.invert) matched = !matched;

        if (matched) {
            ++match_count;
            status = 0;
            if (opts.quiet) return 0;
            if (opts.files_only) {
                write_str(1, fname);
                write_all(1, "\n", 1);
                return 0;
            }
            if (!opts.count) {
                if (show_name) { write_str(1, fname); write_all(1, ":", 1); }
                if (opts.number) { write_ulong(1, line_no); write_all(1, ":", 1); }
                write_all(1, line, line_len);
                write_all(1, "\n", 1);
            }
        }
    }

    if (opts.count) {
        if (show_name) { write_str(1, fname); write_all(1, ":", 1); }
        write_ulong(1, match_count);
        write_all(1, "\n", 1);
        if (match_count > 0) status = 0;
    }

    return status;
}

} // namespace

int main(int argc, char** argv) {
    Opts opts{};
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        for (int j = 1; argv[argi][j] != '\0'; ++j) {
            switch (argv[argi][j]) {
            case 'i': opts.icase = true; break;
            case 'v': opts.invert = true; break;
            case 'c': opts.count = true; break;
            case 'n': opts.number = true; break;
            case 'l': opts.files_only = true; break;
            case 'q': opts.quiet = true; break;
            default:
                write_str(2, "grep: unknown option -");
                write_all(2, &argv[argi][j], 1);
                write_all(2, "\n", 1);
                return 2;
            }
        }
        ++argi;
    }

    if (argi >= argc) {
        write_str(2, "usage: grep [-ivcnlq] pattern [file ...]\n");
        return 2;
    }

    const char* pattern = argv[argi++];

    if (argi >= argc) {
        // Read from stdin
        return grep_fd(0, pattern, "(stdin)", false, opts);
    }

    int status = 1;
    bool multi = (argc - argi > 1);
    for (int i = argi; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "grep: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 2;
                continue;
            }
        }
        if (grep_fd(fd, pattern, argv[i], multi, opts) == 0)
            status = 0;
        if (fd != 0) close(fd);
    }
    return status;
}
