// cut -- remove sections from each line of files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -d DELIM (delimiter, default tab), -f LIST (fields), -c LIST (characters).
// LIST format: N, N-M, N-, -M, comma-separated.

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kLineMax = 4096;
constexpr int kMaxSelect = 1024; // max column/field index we track

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

// Bitmap of selected positions (1-based)
bool g_selected[kMaxSelect + 1];

// Parse a list like "1,3-5,7-" into the g_selected bitmap.
bool parse_list(const char* s) {
    memset(g_selected, 0, sizeof(g_selected));

    while (*s != '\0') {
        int lo = 0;
        int hi = 0;
        bool has_dash = false;

        // Parse first number
        if (*s >= '0' && *s <= '9') {
            while (*s >= '0' && *s <= '9') {
                lo = lo * 10 + (*s - '0');
                ++s;
            }
        }

        if (*s == '-') {
            has_dash = true;
            ++s;
            if (*s >= '0' && *s <= '9') {
                while (*s >= '0' && *s <= '9') {
                    hi = hi * 10 + (*s - '0');
                    ++s;
                }
            } else {
                hi = kMaxSelect; // open-ended range
            }
        }

        if (!has_dash) {
            // Single number
            if (lo >= 1 && lo <= kMaxSelect)
                g_selected[lo] = true;
        } else {
            // Range
            if (lo == 0) lo = 1;
            if (hi > kMaxSelect) hi = kMaxSelect;
            for (int i = lo; i <= hi; ++i)
                g_selected[i] = true;
        }

        if (*s == ',') ++s;
    }
    return true;
}

enum Mode { kNone, kFields, kChars };

void cut_fields(const char* line, int len, char delim) {
    // Split line into fields and output selected ones
    int field = 1;
    int fstart = 0;
    bool first_out = true;
    bool has_delim = false;

    // Check if line contains delimiter at all
    for (int i = 0; i < len; ++i) {
        if (line[i] == delim) { has_delim = true; break; }
    }

    // Per POSIX: if no delimiter in line, print entire line unchanged
    if (!has_delim) {
        write_all(1, line, len);
        write_all(1, "\n", 1);
        return;
    }

    for (int i = 0; i <= len; ++i) {
        if (i == len || line[i] == delim) {
            if (field <= kMaxSelect && g_selected[field]) {
                if (!first_out) write_all(1, &delim, 1);
                write_all(1, line + fstart, i - fstart);
                first_out = false;
            }
            ++field;
            fstart = i + 1;
        }
    }
    write_all(1, "\n", 1);
}

void cut_chars(const char* line, int len) {
    for (int i = 0; i < len; ++i) {
        int pos = i + 1; // 1-based
        if (pos <= kMaxSelect && g_selected[pos])
            write_all(1, &line[i], 1);
    }
    write_all(1, "\n", 1);
}

int process_fd(int fd, Mode mode, char delim) {
    char buf[kBufSize];
    char line[kLineMax];
    int line_len = 0;
    int buf_pos = 0;
    int buf_end = 0;

    for (;;) {
        if (buf_pos >= buf_end) {
            auto n = read(fd, buf, sizeof(buf));
            if (n <= 0) {
                if (line_len > 0) goto process;
                break;
            }
            buf_pos = 0;
            buf_end = static_cast<int>(n);
        }

        {
            char ch = buf[buf_pos++];
            if (ch == '\n' || line_len >= kLineMax - 1) {
                goto process;
            }
            line[line_len++] = ch;
            continue;
        }

    process:
        line[line_len] = '\0';
        if (mode == kFields) cut_fields(line, line_len, delim);
        else                  cut_chars(line, line_len);
        line_len = 0;
        if (buf_pos > buf_end) break;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Mode mode = kNone;
    char delim = '\t';
    int first_file = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'd') {
            // -d DELIM or -dDELIM
            if (argv[i][2] != '\0') {
                delim = argv[i][2];
            } else if (i + 1 < argc) {
                delim = argv[++i][0];
            }
        } else if (argv[i][0] == '-' && argv[i][1] == 'f') {
            mode = kFields;
            const char* list = nullptr;
            if (argv[i][2] != '\0') {
                list = &argv[i][2];
            } else if (i + 1 < argc) {
                list = argv[++i];
            }
            if (list) parse_list(list);
        } else if (argv[i][0] == '-' && argv[i][1] == 'c') {
            mode = kChars;
            const char* list = nullptr;
            if (argv[i][2] != '\0') {
                list = &argv[i][2];
            } else if (i + 1 < argc) {
                list = argv[++i];
            }
            if (list) parse_list(list);
        } else if (argv[i][0] != '-' || (argv[i][0] == '-' && argv[i][1] == '\0')) {
            first_file = i;
            break;
        }
    }

    if (mode == kNone) {
        write_str(2, "usage: cut -f LIST | -c LIST [-d DELIM] [file ...]\n");
        return 1;
    }

    if (first_file == 0) {
        return process_fd(0, mode, delim);
    }

    int status = 0;
    for (int i = first_file; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "cut: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (process_fd(fd, mode, delim) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
