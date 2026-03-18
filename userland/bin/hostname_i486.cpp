// hostname -- print system hostname (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Uses uname() to retrieve the nodename.

#include <sys/utsname.h>
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

int main() {
    struct utsname u{};
    if (uname(&u) != 0) {
        write_str(2, "hostname: uname failed\n");
        return 1;
    }
    write_str(1, u.nodename);
    write_str(1, "\n");
    return 0;
}
