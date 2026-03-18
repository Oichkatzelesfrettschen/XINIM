// readlink -- print value of a symbolic link (common UNIX)
// Cleanroom C++23 implementation.

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
    if (argc < 2) {
        write_str(2, "usage: readlink path\n");
        return 1;
    }

    char buf[1024];
    auto n = readlink(argv[1], buf, sizeof(buf) - 1);
    if (n < 0) {
        write_str(2, "readlink: ");
        write_str(2, argv[1]);
        write_str(2, ": not a symbolic link\n");
        return 1;
    }

    buf[n] = '\0';
    write_str(1, buf);
    write_str(1, "\n");
    return 0;
}
