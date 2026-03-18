// sleep -- suspend execution for an interval (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

#include <time.h>
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

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: sleep seconds\n");
        return 1;
    }

    long secs = 0;
    for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
        secs = secs * 10 + (*p - '0');

    if (secs <= 0) return 0;

    struct timespec req{};
    req.tv_sec = secs;
    req.tv_nsec = 0;
    nanosleep(&req, nullptr);
    return 0;
}
