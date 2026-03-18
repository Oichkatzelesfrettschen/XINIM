// tac -- concatenate and print files in reverse line order (GNU extension)
// Cleanroom C++23 implementation.

#include <fcntl.h>
#include <stdlib.h>
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

int tac_fd(int fd) {
    int len = 0;
    char* buf = read_all(fd, &len);
    if (!buf) {
        write_str(2, "tac: read error\n");
        return 1;
    }
    if (len == 0) { free(buf); return 0; }

    // Collect line start positions
    // Lines end with '\n'; the last "line" may lack one.
    int cap = 256;
    int nlines = 0;
    auto* starts = static_cast<int*>(malloc(static_cast<unsigned>(cap) * sizeof(int)));
    if (!starts) { free(buf); return 1; }

    starts[nlines++] = 0;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n' && i + 1 < len) {
            if (nlines >= cap) {
                cap *= 2;
                auto* ns = static_cast<int*>(realloc(starts, static_cast<unsigned>(cap) * sizeof(int)));
                if (!ns) { free(starts); free(buf); return 1; }
                starts = ns;
            }
            starts[nlines++] = i + 1;
        }
    }

    // Output lines in reverse
    for (int i = nlines - 1; i >= 0; --i) {
        int start = starts[i];
        int end;
        if (i + 1 < nlines)
            end = starts[i + 1];
        else
            end = len;
        write_all(1, buf + start, end - start);
        // If the last line didn't end with newline, add one
        if (buf[end - 1] != '\n')
            write_all(1, "\n", 1);
    }

    free(starts);
    free(buf);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc <= 1) {
        return tac_fd(0);
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "tac: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (tac_fd(fd) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
