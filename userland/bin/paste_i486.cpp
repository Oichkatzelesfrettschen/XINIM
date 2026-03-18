// paste -- merge corresponding lines of files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -d LIST (delimiter characters, default tab).
// Merges lines from files side by side; - means stdin.

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kMaxFiles = 64;

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

// Read one line from fd into buf (without newline). Returns:
//  >0 = line length, 0 = empty line (just newline), -1 = EOF
int read_line(int fd, char* line, int max) {
    int len = 0;
    char ch;
    for (;;) {
        auto n = read(fd, &ch, 1);
        if (n <= 0) {
            return (len > 0) ? len : -1;
        }
        if (ch == '\n') return len;
        if (len < max - 1)
            line[len++] = ch;
    }
}

} // namespace

int main(int argc, char** argv) {
    const char* delims = "\t";
    int fds[kMaxFiles];
    bool eof[kMaxFiles];
    int nfds = 0;
    int argi = 1;

    // Parse options
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        if (argv[argi][1] == 'd') {
            if (argv[argi][2] != '\0') {
                delims = &argv[argi][2];
            } else if (argi + 1 < argc) {
                delims = argv[++argi];
            }
        } else if (argv[argi][1] == '-' && argv[argi][2] == '\0') {
            // "--" end of options
            ++argi;
            break;
        } else {
            // "-" is stdin, not an option
            break;
        }
        ++argi;
    }

    // Open files
    for (int i = argi; i < argc && nfds < kMaxFiles; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fds[nfds] = 0;
        } else {
            fds[nfds] = open(argv[i], O_RDONLY, 0);
            if (fds[nfds] < 0) {
                write_str(2, "paste: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                return 1;
            }
        }
        eof[nfds] = false;
        ++nfds;
    }

    if (nfds == 0) {
        // No files: read stdin
        fds[0] = 0;
        eof[0] = false;
        nfds = 1;
    }

    int dlen = str_len(delims);
    if (dlen == 0) { delims = "\t"; dlen = 1; }

    char line[kBufSize];

    for (;;) {
        bool all_done = true;

        for (int i = 0; i < nfds; ++i) {
            if (i > 0) {
                // Output delimiter (cycle through delimiter list)
                int di = (i - 1) % dlen;
                write_all(1, &delims[di], 1);
            }

            if (!eof[i]) {
                int len = read_line(fds[i], line, kBufSize);
                if (len < 0) {
                    eof[i] = true;
                } else {
                    all_done = false;
                    if (len > 0)
                        write_all(1, line, len);
                }
            }
        }

        if (all_done) break;
        write_all(1, "\n", 1);
    }

    for (int i = 0; i < nfds; ++i) {
        if (fds[i] > 0) close(fds[i]);
    }

    return 0;
}
