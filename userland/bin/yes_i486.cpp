// yes -- output a string repeatedly until killed (POSIX-adjacent)
// Cleanroom C++23 implementation.
// Default output: "y\n". With arguments: concatenated args + "\n".

#include <unistd.h>

namespace {

void write_all(int fd, const char* buf, int len) {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) _exit(1);
        buf += w;
        len -= static_cast<int>(w);
    }
}

int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

} // namespace

int main(int argc, char** argv) {
    // Build the output line into a buffer for efficiency
    char buf[4096];
    int len = 0;

    if (argc <= 1) {
        buf[0] = 'y';
        buf[1] = '\n';
        len = 2;
    } else {
        for (int i = 1; i < argc; ++i) {
            if (i > 1 && len < 4094) buf[len++] = ' ';
            int slen = str_len(argv[i]);
            for (int j = 0; j < slen && len < 4094; ++j)
                buf[len++] = argv[i][j];
        }
        if (len < 4095) buf[len++] = '\n';
    }

    // Fill a larger buffer with repeated copies for fewer syscalls
    char big[4096];
    int blen = 0;
    while (blen + len <= 4096) {
        for (int i = 0; i < len; ++i)
            big[blen++] = buf[i];
    }

    for (;;) {
        write_all(1, big, blen);
    }
}
