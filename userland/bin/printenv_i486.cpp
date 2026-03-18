// printenv -- print all or named environment variables (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

namespace {

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

} // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        // Print named variables
        int status = 0;
        for (int i = 1; i < argc; ++i) {
            const char* val = getenv(argv[i]);
            if (val != nullptr) {
                write_str(1, val);
                write_str(1, "\n");
            } else {
                status = 1;
            }
        }
        return status;
    }

    // Print all environment variables
    if (environ != nullptr) {
        for (char** ep = environ; *ep != nullptr; ++ep) {
            write_str(1, *ep);
            write_str(1, "\n");
        }
    }
    return 0;
}
