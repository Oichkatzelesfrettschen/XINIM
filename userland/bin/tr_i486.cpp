// tr -- translate or delete characters (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -d (delete characters in set1), -s (squeeze repeated output chars).
// Reads stdin, writes stdout. Builds a 256-byte translation table.

#include <string.h>
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

int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// Expand escape sequences and ranges in a set string.
// "a-z" expands to all chars from a to z.
// \n, \t, \\ are recognized.
// Returns expanded length, writes into out (max out_cap).
int expand_set(const char* s, unsigned char* out, int out_cap) {
    int n = 0;
    for (int i = 0; s[i] != '\0' && n < out_cap; ++i) {
        if (s[i] == '\\' && s[i + 1] != '\0') {
            ++i;
            switch (s[i]) {
            case 'n': out[n++] = '\n'; break;
            case 't': out[n++] = '\t'; break;
            case 'r': out[n++] = '\r'; break;
            case '\\': out[n++] = '\\'; break;
            case 'a': out[n++] = '\a'; break;
            case 'b': out[n++] = '\b'; break;
            case 'f': out[n++] = '\f'; break;
            case 'v': out[n++] = '\v'; break;
            default: out[n++] = static_cast<unsigned char>(s[i]); break;
            }
        } else if (s[i + 1] == '-' && s[i + 2] != '\0') {
            // Character range: a-z
            unsigned char lo = static_cast<unsigned char>(s[i]);
            unsigned char hi = static_cast<unsigned char>(s[i + 2]);
            i += 2;
            if (lo <= hi) {
                for (unsigned char c = lo; c <= hi && n < out_cap; ++c)
                    out[n++] = c;
            } else {
                for (unsigned char c = lo; c >= hi && n < out_cap; --c) {
                    out[n++] = c;
                    if (c == 0) break; // prevent underflow
                }
            }
        } else {
            out[n++] = static_cast<unsigned char>(s[i]);
        }
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    bool delete_mode = false;
    bool squeeze = false;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        for (int j = 1; argv[argi][j] != '\0'; ++j) {
            switch (argv[argi][j]) {
            case 'd': delete_mode = true; break;
            case 's': squeeze = true; break;
            default: break;
            }
        }
        ++argi;
    }

    if (argi >= argc) {
        write_str(2, "usage: tr [-ds] string1 [string2]\n");
        return 1;
    }

    // Expand set1
    unsigned char set1[256];
    int set1_len = expand_set(argv[argi++], set1, 256);

    // Expand set2 if present
    unsigned char set2[256];
    int set2_len = 0;
    if (argi < argc)
        set2_len = expand_set(argv[argi], set2, 256);

    // Build translation table and membership set
    unsigned char map[256];
    bool in_set1[256];
    memset(in_set1, 0, sizeof(in_set1));
    for (int i = 0; i < 256; ++i)
        map[i] = static_cast<unsigned char>(i);

    for (int i = 0; i < set1_len; ++i) {
        unsigned char c = set1[i];
        in_set1[c] = true;
        if (!delete_mode && set2_len > 0) {
            // Map to corresponding set2 char; if set2 is shorter, use last char
            unsigned char r = (i < set2_len) ? set2[i] : set2[set2_len - 1];
            map[c] = r;
        }
    }

    // Build squeeze set: chars in set2 (or set1 if -d -s with delete+squeeze)
    bool squeeze_set[256];
    memset(squeeze_set, 0, sizeof(squeeze_set));
    if (squeeze) {
        if (delete_mode) {
            // -d -s: delete set1, squeeze set2
            for (int i = 0; i < set2_len; ++i)
                squeeze_set[set2[i]] = true;
        } else {
            // -s without -d: squeeze output chars that came from set1 mapping
            for (int i = 0; i < set1_len; ++i)
                squeeze_set[map[set1[i]]] = true;
        }
    }

    char buf[kBufSize];
    char out[kBufSize];
    int out_len = 0;
    int last_out = -1; // last output char for squeeze detection

    for (;;) {
        auto n = read(0, buf, sizeof(buf));
        if (n <= 0) break;

        out_len = 0;
        for (int i = 0; i < static_cast<int>(n); ++i) {
            unsigned char c = static_cast<unsigned char>(buf[i]);

            if (delete_mode && in_set1[c]) continue;

            unsigned char oc = map[c];

            if (squeeze && squeeze_set[oc] && oc == static_cast<unsigned char>(last_out))
                continue;

            out[out_len++] = static_cast<char>(oc);
            last_out = oc;

            if (out_len >= kBufSize) {
                write_all(1, out, out_len);
                out_len = 0;
            }
        }

        if (out_len > 0)
            write_all(1, out, out_len);
    }

    return 0;
}
