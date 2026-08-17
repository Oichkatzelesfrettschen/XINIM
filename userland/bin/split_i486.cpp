// split -- split a file into pieces (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports: -l LINES (default 1000). Output files named xaa, xab, etc.

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

// Generate output filename: prefix + two letters (aa, ab, ac, ... zz)
void make_name(char* out, const char* prefix, int seq) {
    int i = 0;
    for (int j = 0; prefix[j] != '\0'; ++j) out[i++] = prefix[j];
    out[i++] = static_cast<char>('a' + seq / 26);
    out[i++] = static_cast<char>('a' + seq % 26);
    out[i] = '\0';
}

} // namespace

int main(int argc, char** argv) {
    int lines_per_file = 1000;
    const char* prefix = "x";
    const char* inpath = nullptr;

    // Parse options
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'l' && argv[i][2] == '\0') {
            if (i + 1 < argc) {
                ++i;
                lines_per_file = 0;
                for (int j = 0; argv[i][j] != '\0'; ++j)
                    lines_per_file = lines_per_file * 10 + (argv[i][j] - '0');
                if (lines_per_file < 1) lines_per_file = 1;
            }
        } else if (argv[i][0] == '-' && argv[i][1] == '\0') {
            // stdin, handled below
            inpath = nullptr;
        } else if (argv[i][0] != '-') {
            if (!inpath) {
                inpath = argv[i];
            } else {
                prefix = argv[i];
            }
        }
    }

    int fdi = 0; // stdin
    if (inpath) {
        fdi = open(inpath, O_RDONLY, 0);
        if (fdi < 0) {
            write_str(2, "split: cannot open ");
            write_str(2, inpath);
            write_str(2, "\n");
            return 1;
        }
    }

    int seq = 0;
    int line_count = 0;
    int fdo = -1;
    char outname[256];
    char buf[kBufSize];

    for (;;) {
        auto nr = read(fdi, buf, sizeof(buf));
        if (nr <= 0) break;

        for (int i = 0; i < static_cast<int>(nr); ++i) {
            // Open new output file if needed
            if (fdo < 0) {
                if (seq >= 676) break; // aa..zz = 676 files max
                make_name(outname, prefix, seq++);
                fdo = open(outname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fdo < 0) {
                    write_str(2, "split: cannot create ");
                    write_str(2, outname);
                    write_str(2, "\n");
                    if (inpath) close(fdi);
                    return 1;
                }
                line_count = 0;
            }

            write_all(fdo, &buf[i], 1);
            if (buf[i] == '\n') {
                ++line_count;
                if (line_count >= lines_per_file) {
                    close(fdo);
                    fdo = -1;
                }
            }
        }
    }

    if (fdo >= 0) close(fdo);
    if (inpath) close(fdi);
    return 0;
}
