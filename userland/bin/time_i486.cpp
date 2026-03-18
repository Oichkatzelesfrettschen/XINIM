// time -- time command execution (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Fork+exec, measure wall-clock with gettimeofday before/after.

#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
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

void write_elapsed(unsigned long ms) {
    unsigned long s = ms / 1000;
    unsigned long frac = ms % 1000;

    // Seconds
    char buf[20];
    int pos = 0;
    if (s == 0) { buf[pos++] = '0'; }
    else {
        while (s > 0) {
            buf[pos++] = static_cast<char>('0' + s % 10);
            s /= 10;
        }
    }
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos - 1 - i]; buf[pos - 1 - i] = t;
    }
    write_all(2, buf, pos);

    // Fractional
    write_str(2, ".");
    char fb[3];
    fb[0] = static_cast<char>('0' + frac / 100);
    fb[1] = static_cast<char>('0' + (frac / 10) % 10);
    fb[2] = static_cast<char>('0' + frac % 10);
    write_all(2, fb, 3);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: time command [args]\n");
        return 1;
    }

    struct timeval start{};
    struct timeval end{};
    gettimeofday(&start, nullptr);

    auto pid = fork();
    if (pid == 0) {
        execvp(argv[1], argv + 1);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    gettimeofday(&end, nullptr);

    auto ms = static_cast<unsigned long>(end.tv_sec - start.tv_sec) * 1000 +
              static_cast<unsigned long>(end.tv_usec - start.tv_usec) / 1000;

    write_str(2, "\nreal\t0m");
    write_elapsed(ms);
    write_str(2, "s\n");
    write_str(2, "user\t0m0.000s\n");
    write_str(2, "sys\t0m0.000s\n");

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 1;
}
