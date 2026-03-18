// nice -- run a program with modified scheduling priority (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Default adjustment: +10. Supports -n ADJUSTMENT.

#include <unistd.h>

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
    int adjustment = 10;
    int cmd_start = 1;

    // Parse -n ADJUSTMENT
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0') {
        adjustment = 0;
        bool neg = false;
        const char* s = argv[2];
        if (*s == '-') { neg = true; ++s; }
        while (*s >= '0' && *s <= '9') {
            adjustment = adjustment * 10 + (*s - '0');
            ++s;
        }
        if (neg) adjustment = -adjustment;
        cmd_start = 3;
    }

    if (cmd_start >= argc) {
        write_str(2, "usage: nice [-n adjustment] command [arguments]\n");
        return 125;
    }

    nice(adjustment);
    execvp(argv[cmd_start], argv + cmd_start);

    write_str(2, "nice: failed to exec ");
    write_str(2, argv[cmd_start]);
    write_str(2, "\n");
    return 126;
}
