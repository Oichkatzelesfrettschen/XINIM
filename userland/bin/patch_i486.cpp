// patch -- apply unified diff patches to files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1 and unified diff format specification.
// Options: -i patchfile, -p N (strip N path components), -R (reverse), --dry-run.

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace {

constexpr int kMaxLines  = 8192;
constexpr int kLineLen   = 1024;
constexpr int kPatchSize = 131072; // 128 KiB max patch
constexpr int kFileSize  = 524288; // 512 KiB max file

void write_all(int fd, const char* buf, int len) noexcept {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w; len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) noexcept {
    int n = 0; while (s[n]) ++n;
    write_all(fd, s, n);
}

void write_int(int fd, int v) noexcept {
    char tmp[16]; int n = 0; bool neg = v < 0;
    if (neg) v = -v;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    if (neg) tmp[n++] = '-';
    char out[16];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

int str_len(const char* s) noexcept { int n = 0; while (s[n]) ++n; return n; }

bool str_eq(const char* a, const char* b) noexcept {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

bool str_starts(const char* s, const char* prefix) noexcept {
    while (*prefix) { if (*s != *prefix) return false; ++s; ++prefix; }
    return true;
}

void str_copy(char* dst, const char* src, int cap) noexcept {
    int i = 0;
    while (i < cap - 1 && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

int str_cmp(const char* a, const char* b) noexcept {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

// Read entire fd into buf (up to bufsz - 1 bytes), NUL-terminate, return length.
int read_all(int fd, char* buf, int bufsz) noexcept {
    int total = 0;
    while (total < bufsz - 1) {
        auto n = read(fd, buf + total, static_cast<unsigned>(bufsz - 1 - total));
        if (n <= 0) break;
        total += static_cast<int>(n);
    }
    buf[total] = '\0';
    return total;
}

// Write buf to path (overwrite).
int write_file(const char* path, const char* data, int len) noexcept {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write_all(fd, data, len);
    close(fd);
    return 0;
}

// Strip N leading path components from path (for -p flag).
const char* strip_prefix(const char* path, int n) noexcept {
    for (int i = 0; i < n && *path; ++i) {
        while (*path && *path != '/') ++path;
        if (*path == '/') ++path;
    }
    return path;
}

// ============================================================
// Line buffer: split buffer into lines (in-place NUL termination)
// ============================================================

struct LineBuf {
    char* lines[kMaxLines];
    int   count;
    char  data[kFileSize];
    int   data_len;

    void load(const char* src, int len) noexcept {
        count = 0;
        data_len = len < kFileSize - 1 ? len : kFileSize - 1;
        for (int i = 0; i < data_len; ++i) data[i] = src[i];
        data[data_len] = '\0';

        char* p = data;
        while (*p && count < kMaxLines) {
            lines[count++] = p;
            while (*p && *p != '\n') ++p;
            if (*p == '\n') *p++ = '\0';
        }
    }

    const char* get(int i) const noexcept {
        if (i < 0 || i >= count) return nullptr;
        return lines[i];
    }
};

// ============================================================
// Output builder
// ============================================================

struct OutBuf {
    char data[kFileSize];
    int  len;

    void init() noexcept { len = 0; }

    void append_line(const char* s) noexcept {
        int n = str_len(s);
        if (len + n + 1 >= kFileSize) return;
        for (int i = 0; i < n; ++i) data[len + i] = s[i];
        len += n;
        data[len++] = '\n';
    }

    void append_line_raw(const char* s, int n) noexcept {
        if (len + n + 1 >= kFileSize) return;
        for (int i = 0; i < n; ++i) data[len + i] = s[i];
        len += n;
        data[len++] = '\n';
    }
};

// ============================================================
// Hunk descriptor
// ============================================================

struct Hunk {
    int  old_start;  // 1-based
    int  old_count;
    int  new_start;
    int  new_count;
    // Lines of the hunk body ('+', '-', ' ' prefixed)
    char lines[256][kLineLen];
    int  nlines;
};

// Parse a @@ -a,b +c,d @@ hunk header.
// p points to the first '@' of "@@".
bool parse_hunk_header(const char*& p, Hunk& h) noexcept {
    if (p[0] != '@' || p[1] != '@') return false;
    p += 2;
    while (*p == ' ') ++p;
    if (*p != '-') return false;
    ++p;

    // Parse old start
    h.old_start = 0;
    while (*p >= '0' && *p <= '9') h.old_start = h.old_start * 10 + (*p++ - '0');
    h.old_count = 1;
    if (*p == ',') { ++p; h.old_count = 0; while (*p >= '0' && *p <= '9') h.old_count = h.old_count * 10 + (*p++ - '0'); }
    while (*p == ' ') ++p;
    if (*p != '+') return false;
    ++p;

    h.new_start = 0;
    while (*p >= '0' && *p <= '9') h.new_start = h.new_start * 10 + (*p++ - '0');
    h.new_count = 1;
    if (*p == ',') { ++p; h.new_count = 0; while (*p >= '0' && *p <= '9') h.new_count = h.new_count * 10 + (*p++ - '0'); }

    // Skip to end of line
    while (*p && *p != '\n') ++p;
    if (*p == '\n') ++p;

    h.nlines = 0;
    return true;
}

// ============================================================
// Apply one hunk to the file (modifying in_cursor and writing to out)
// Returns false on error or fuzz-exceeded.
// ============================================================

bool apply_hunk(const Hunk& h, LineBuf& in, int& in_cursor, OutBuf& out,
                bool reverse) noexcept {
    // Determine where in the input we expect this hunk to start (0-based).
    int expected = (reverse ? h.new_start : h.old_start) - 1;
    int expected_count = reverse ? h.new_count : h.old_count;
    static_cast<void>(expected_count);

    // Fuzz search: look forward up to 5 lines if context doesn't match exactly.
    int fuzz = 0;
    int target = expected;
    while (fuzz <= 5) {
        bool ok = true;
        int scan = target;
        for (int i = 0; i < h.nlines && ok; ++i) {
            char pref = h.lines[i][0];
            if (reverse) pref = (pref == '+') ? '-' : (pref == '-') ? '+' : ' ';
            if (pref == ' ' || pref == '-') {
                const char* src_line = in.get(scan);
                if (src_line == nullptr || !str_eq(src_line, h.lines[i] + 1)) {
                    ok = false;
                }
                ++scan;
            }
        }
        if (ok) { target = target; break; }
        ++fuzz; ++target;
        if (fuzz > 5) {
            write_str(2, "patch: hunk failed at line ");
            write_int(2, expected + 1);
            write_str(2, "\n");
            return false;
        }
    }

    // Copy unchanged lines before this hunk
    while (in_cursor < target) {
        const char* l = in.get(in_cursor);
        if (l) out.append_line(l);
        ++in_cursor;
    }

    // Apply hunk lines
    for (int i = 0; i < h.nlines; ++i) {
        char pref = h.lines[i][0];
        if (reverse) pref = (pref == '+') ? '-' : (pref == '-') ? '+' : ' ';

        if (pref == ' ') {
            // Context: copy from source
            const char* l = in.get(in_cursor);
            if (l) out.append_line(l);
            ++in_cursor;
        } else if (pref == '-') {
            // Remove: advance source without outputting
            ++in_cursor;
        } else if (pref == '+') {
            // Add: output new line
            out.append_line(h.lines[i] + 1);
        }
    }
    return true;
}

} // namespace

// Large static buffers (avoid stack allocation)
static char g_patch_buf[kPatchSize];
static char g_file_buf[kFileSize];
static LineBuf g_in_lines;
static OutBuf  g_out;
static Hunk    g_hunk;

int main(int argc, char** argv) {
    const char* patch_file = nullptr;
    int strip_n = 0;
    bool reverse = false;
    bool dry_run = false;
    bool backup = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (str_eq(a, "-i") && i + 1 < argc) { patch_file = argv[++i]; }
        else if (str_eq(a, "-R") || str_eq(a, "--reverse")) { reverse = true; }
        else if (str_eq(a, "--dry-run")) { dry_run = true; }
        else if (str_eq(a, "-b") || str_eq(a, "--backup")) { backup = true; }
        else if (a[0] == '-' && a[1] == 'p') {
            strip_n = 0;
            const char* p = a + 2;
            if (*p == '\0' && i + 1 < argc) p = argv[++i];
            while (*p >= '0' && *p <= '9') strip_n = strip_n * 10 + (*p++ - '0');
        } else if (a[0] != '-') {
            // Positional arg: may be a patch file or a target file
            if (patch_file == nullptr) patch_file = a;
        }
    }

    // Read patch from patch_file or stdin
    int pfd = 0;
    if (patch_file != nullptr) {
        pfd = open(patch_file, O_RDONLY, 0);
        if (pfd < 0) {
            write_str(2, "patch: cannot open patch file: ");
            write_str(2, patch_file); write_str(2, "\n");
            return 1;
        }
    }
    int patch_len = read_all(pfd, g_patch_buf, kPatchSize);
    if (pfd > 0) close(pfd);
    static_cast<void>(patch_len);

    // Parse patch: iterate over diff sections (--- / +++ header pairs)
    const char* p = g_patch_buf;
    int n_applied = 0;
    int n_failed  = 0;

    while (*p) {
        // Skip to next --- line
        while (*p) {
            if (str_starts(p, "--- ")) break;
            while (*p && *p != '\n') ++p;
            if (*p == '\n') ++p;
        }
        if (!*p) break;

        // Parse --- and +++ file lines
        p += 4; // skip "--- "
        char old_name[256]{}; int on = 0;
        // Strip "a/" prefix (git format)
        if (p[0] == 'a' && p[1] == '/') p += 2;
        while (*p && *p != '\n' && *p != '\t' && on < 255) old_name[on++] = *p++;
        old_name[on] = '\0';
        while (*p && *p != '\n') ++p;
        if (*p == '\n') ++p;

        if (!str_starts(p, "+++ ")) continue;
        p += 4;
        char new_name[256]{}; int nn = 0;
        if (p[0] == 'b' && p[1] == '/') p += 2;
        while (*p && *p != '\n' && *p != '\t' && nn < 255) new_name[nn++] = *p++;
        new_name[nn] = '\0';
        while (*p && *p != '\n') ++p;
        if (*p == '\n') ++p;

        // Determine target file
        const char* target_raw = reverse ? old_name : new_name;
        const char* target = strip_prefix(target_raw, strip_n);

        write_str(1, "patching file "); write_str(1, target); write_str(1, "\n");

        // Read target file into line buffer
        int tfd = open(target, O_RDONLY, 0);
        int file_len = 0;
        if (tfd >= 0) {
            file_len = read_all(tfd, g_file_buf, kFileSize);
            close(tfd);
        } else {
            g_file_buf[0] = '\0';
        }
        g_in_lines.load(g_file_buf, file_len);
        g_out.init();

        // Create backup if requested
        if (backup && tfd >= 0 && !dry_run) {
            char bak[260]; str_copy(bak, target, 257);
            int bl = str_len(bak); bak[bl] = '.'; bak[bl+1] = 'o'; bak[bl+2] = 'r'; bak[bl+3] = 'i'; bak[bl+4] = 'g'; bak[bl+5] = '\0';
            write_file(bak, g_file_buf, file_len);
        }

        // Apply all hunks for this file
        int in_cursor = 0;
        bool file_ok = true;
        int hunk_n = 0;

        while (*p && p[0] == '@' && p[1] == '@') {
            Hunk& h = g_hunk;
            h.nlines = 0;
            if (!parse_hunk_header(p, h)) { write_str(2, "patch: malformed hunk header\n"); break; }
            ++hunk_n;

            // Read hunk body lines
            while (*p && !(p[0] == '@' && p[1] == '@') &&
                   !str_starts(p, "--- ") && !str_starts(p, "diff ")) {
                if (h.nlines >= 256) { while (*p && *p != '\n') ++p; if (*p == '\n') ++p; continue; }
                char pref = p[0];
                if (pref == '+' || pref == '-' || pref == ' ' || pref == '\\') {
                    ++p;
                    int ln = 0;
                    h.lines[h.nlines][0] = pref;
                    while (*p && *p != '\n' && ln < kLineLen - 2) h.lines[h.nlines][ln++ + 1] = *p++;
                    h.lines[h.nlines][ln + 1] = '\0';
                    if (pref != '\\') ++h.nlines;
                    while (*p && *p != '\n') ++p;
                    if (*p == '\n') ++p;
                } else {
                    while (*p && *p != '\n') ++p;
                    if (*p == '\n') ++p;
                }
            }

            if (!apply_hunk(h, g_in_lines, in_cursor, g_out, reverse)) {
                file_ok = false; ++n_failed;
            }
        }

        // Copy any remaining input lines
        while (in_cursor < g_in_lines.count) {
            const char* l = g_in_lines.get(in_cursor++);
            if (l) g_out.append_line(l);
        }

        if (file_ok) {
            ++n_applied;
            if (!dry_run) {
                if (write_file(target, g_out.data, g_out.len) < 0) {
                    write_str(2, "patch: cannot write: "); write_str(2, target); write_str(2, "\n");
                    ++n_failed;
                }
            }
        }
    }

    if (n_failed > 0) {
        write_str(2, "patch: "); write_int(2, n_failed); write_str(2, " hunk(s) failed\n");
        return 1;
    }
    return 0;
}
