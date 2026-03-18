// chmod -- change file mode bits (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports octal modes (e.g. "755"). Symbolic modes deferred.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    while (n > 0) {
        auto w = write(fd, s, static_cast<unsigned>(n));
        if (w <= 0) return;
        s += w;
        n -= static_cast<int>(w);
    }
}

void err(const char* prog, const char* msg) {
    write_str(2, prog);
    write_str(2, ": ");
    write_str(2, msg);
    write_str(2, "\n");
}

void err2(const char* prog, const char* path, const char* msg) {
    write_str(2, prog);
    write_str(2, ": ");
    write_str(2, path);
    write_str(2, ": ");
    write_str(2, msg);
    write_str(2, "\n");
}

// Parse an octal mode string into a mode_t.
// Returns -1 on invalid input.
int parse_octal(const char* s) {
    unsigned mode = 0;
    if (*s == '\0') return -1;
    for (int i = 0; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '7') return -1;
        mode = (mode << 3) | static_cast<unsigned>(s[i] - '0');
    }
    if (mode > 07777) return -1;
    return static_cast<int>(mode);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        err("chmod", "usage: chmod mode file...");
        return 1;
    }

    const char* mode_str = argv[1];
    int mode = parse_octal(mode_str);
    if (mode < 0) {
        err("chmod", "invalid mode (use octal, e.g. 755)");
        return 1;
    }

    int status = 0;
    for (int i = 2; i < argc; ++i) {
        if (chmod(argv[i], static_cast<unsigned>(mode)) < 0) {
            err2("chmod", argv[i], "cannot change mode");
            status = 1;
        }
    }

    return status;
}
