// dirname -- strip last component from file name (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

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

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        const char msg[] = "usage: dirname path\n";
        write_all(2, msg, sizeof(msg) - 1);
        return 1;
    }

    const char* path = argv[1];
    int len = slen(path);

    // Strip trailing slashes
    while (len > 1 && path[len - 1] == '/') --len;

    // Find last slash
    int last_slash = -1;
    for (int i = len - 1; i >= 0; --i) {
        if (path[i] == '/') { last_slash = i; break; }
    }

    if (last_slash < 0) {
        // No slash: dirname is "."
        write_all(1, ".\n", 2);
    } else if (last_slash == 0) {
        // Root
        write_all(1, "/\n", 2);
    } else {
        // Strip trailing slashes from dirname
        int end = last_slash;
        while (end > 1 && path[end - 1] == '/') --end;
        write_all(1, path, end);
        write_all(1, "\n", 1);
    }

    return 0;
}
