// tee -- duplicate standard input (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -a (append mode)

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kMaxFiles = 64;

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
    bool append = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                if (argv[i][j] == 'a') append = true;
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    int fds[kMaxFiles];
    int nfds = 0;

    int flags = O_WRONLY | O_CREAT;
    if (append)
        flags |= O_APPEND;
    else
        flags |= O_TRUNC;

    for (int i = first_arg; i < argc && nfds < kMaxFiles; ++i) {
        int fd = open(argv[i], flags, 0666);
        if (fd < 0) {
            write_str(2, "tee: ");
            write_str(2, argv[i]);
            write_str(2, ": cannot open\n");
            continue;
        }
        fds[nfds++] = fd;
    }

    char buf[kBufSize];
    int status = 0;

    for (;;) {
        auto n = read(0, buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) { status = 1; break; }

        int len = static_cast<int>(n);
        write_all(1, buf, len);

        for (int i = 0; i < nfds; ++i) {
            write_all(fds[i], buf, len);
        }
    }

    for (int i = 0; i < nfds; ++i) {
        close(fds[i]);
    }

    return status;
}
