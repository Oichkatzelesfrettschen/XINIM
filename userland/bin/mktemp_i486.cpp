// mktemp -- create a temporary file or directory (common UNIX)
// Cleanroom C++23 implementation.
// Generates name from template (XXXXXX replaced with pseudo-random chars).
// Uses PID and a simple counter for entropy.

#include <fcntl.h>
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

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// Simple pseudo-random character from PID-seeded LCG
unsigned g_seed;

char rand_char() {
    g_seed = g_seed * 1103515245 + 12345;
    unsigned idx = (g_seed >> 16) % 62;
    if (idx < 26) return static_cast<char>('a' + idx);
    if (idx < 52) return static_cast<char>('A' + idx - 26);
    return static_cast<char>('0' + idx - 52);
}

} // namespace

int main(int argc, char** argv) {
    const char* tmpl = "/tmp/tmp.XXXXXX";
    if (argc >= 2) tmpl = argv[1];

    // Seed with PID
    g_seed = static_cast<unsigned>(getpid()) * 31337;

    // Copy template and replace trailing X's
    char path[256];
    int len = slen(tmpl);
    if (len >= static_cast<int>(sizeof(path))) len = sizeof(path) - 1;
    for (int i = 0; i < len; ++i) path[i] = tmpl[i];
    path[len] = '\0';

    // Find trailing X's
    int xs_end = len;
    int xs_start = len;
    while (xs_start > 0 && path[xs_start - 1] == 'X') --xs_start;

    if (xs_start == xs_end) {
        write_str(2, "mktemp: template must end with X's\n");
        return 1;
    }

    // Try up to 100 times
    for (int attempt = 0; attempt < 100; ++attempt) {
        for (int i = xs_start; i < xs_end; ++i)
            path[i] = rand_char();

        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            close(fd);
            write_str(1, path);
            write_str(1, "\n");
            return 0;
        }
    }

    write_str(2, "mktemp: failed to create temporary file\n");
    return 1;
}
