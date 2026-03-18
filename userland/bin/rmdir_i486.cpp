// rmdir -- remove empty directories (POSIX.1)
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

void err(const char* prog, const char* msg) {
    write_str(2, prog);
    write_str(2, ": ");
    write_str(2, msg);
    write_str(2, "\n");
}

void err2(const char* prog, const char* path, const char* msg) {
    write_str(2, prog);
    write_str(2, ": ");
    write_str(2, path);
    write_str(2, ": ");
    write_str(2, msg);
    write_str(2, "\n");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        err("rmdir", "missing operand");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        // Skip "--" sentinel
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == '\0')
            continue;

        if (rmdir(argv[i]) < 0) {
            err2("rmdir", argv[i], "failed to remove directory");
            status = 1;
        }
    }

    return status;
}
