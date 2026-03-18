// basename -- strip directory and suffix from filenames (POSIX.1)
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
        const char msg[] = "usage: basename path [suffix]\n";
        write_all(2, msg, sizeof(msg) - 1);
        return 1;
    }

    const char* path = argv[1];
    int len = slen(path);

    // Strip trailing slashes
    while (len > 1 && path[len - 1] == '/') --len;

    // All slashes => "/"
    if (len == 1 && path[0] == '/') {
        write_all(1, "/\n", 2);
        return 0;
    }

    // Find last slash
    int start = 0;
    for (int i = len - 1; i >= 0; --i) {
        if (path[i] == '/') { start = i + 1; break; }
    }

    int base_len = len - start;

    // Strip suffix if provided
    if (argc >= 3) {
        const char* suffix = argv[2];
        int suf_len = slen(suffix);
        if (suf_len > 0 && suf_len < base_len) {
            bool match = true;
            for (int i = 0; i < suf_len; ++i) {
                if (path[start + base_len - suf_len + i] != suffix[i]) {
                    match = false;
                    break;
                }
            }
            if (match) base_len -= suf_len;
        }
    }

    write_all(1, path + start, base_len);
    write_all(1, "\n", 1);
    return 0;
}
