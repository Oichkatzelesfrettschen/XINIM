// env -- set environment and execute command, or print environment (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

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

bool has_equals(const char* s) {
    while (*s != '\0') {
        if (*s == '=') return true;
        ++s;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    bool ignore_env = false;
    int argi = 1;

    // Parse options
    while (argi < argc && argv[argi][0] == '-') {
        if (argv[argi][1] == 'i' && argv[argi][2] == '\0') {
            ignore_env = true;
            ++argi;
        } else if (argv[argi][1] == '-' && argv[argi][2] == '\0') {
            ++argi;
            break;
        } else {
            break;
        }
    }

    // Clear environment if -i
    if (ignore_env) {
        static char* empty_env[] = {nullptr};
        environ = empty_env;
    }

    // Set VAR=VALUE pairs
    while (argi < argc && has_equals(argv[argi])) {
        putenv(argv[argi]);
        ++argi;
    }

    // If remaining args, exec command
    if (argi < argc) {
        execvp(argv[argi], argv + argi);
        write_str(2, "env: ");
        write_str(2, argv[argi]);
        write_str(2, ": exec failed\n");
        return 127;
    }

    // No command: print environment
    if (environ != nullptr) {
        for (char** ep = environ; *ep != nullptr; ++ep) {
            write_str(1, *ep);
            write_str(1, "\n");
        }
    }
    return 0;
}
