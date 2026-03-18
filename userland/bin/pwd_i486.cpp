// pwd -- print working directory (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

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

int main(int, char**) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == nullptr) {
        write_str(2, "pwd: cannot determine working directory\n");
        return 1;
    }
    write_str(1, buf);
    write_str(1, "\n");
    return 0;
}
