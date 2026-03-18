// nohup -- run a command immune to hangups (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Ignores SIGHUP, redirects stdout/stderr to nohup.out if terminal.

#include <fcntl.h>
#include <signal.h>
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
        write_str(2, "usage: nohup command [arguments]\n");
        return 127;
    }

    // Ignore SIGHUP
    signal(SIGHUP, SIG_IGN);

    // If stdout is a terminal, redirect to nohup.out
    if (isatty(1)) {
        int fd = open("nohup.out", O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0) {
            write_str(2, "nohup: cannot open nohup.out\n");
            return 127;
        }
        dup2(fd, 1);
        close(fd);
        write_str(2, "nohup: appending output to nohup.out\n");
    }

    // If stderr is a terminal, redirect to stdout
    if (isatty(2)) {
        dup2(1, 2);
    }

    execvp(argv[1], argv + 1);

    // If exec fails
    write_str(2, "nohup: failed to exec ");
    write_str(2, argv[1]);
    write_str(2, "\n");
    return 127;
}
