// touch -- change file access and modification times (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Creates file if missing; opens and closes to update mtime.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
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
    int first_arg = 1;

    // Skip options we do not yet handle (silently accept -c for no-create)
    bool no_create = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                if (argv[i][j] == 'c') no_create = true;
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    if (first_arg >= argc) {
        write_str(2, "touch: missing operand\n");
        return 1;
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        const char* path = argv[i];
        struct stat st;

        if (stat(path, &st) < 0) {
            // File does not exist
            if (no_create) continue;
            int fd = open(path, O_CREAT | O_WRONLY, 0644);
            if (fd < 0) {
                err2("touch", path, "cannot create");
                status = 1;
                continue;
            }
            close(fd);
        } else {
            // File exists -- open and close to update mtime via kernel
            int fd = open(path, O_WRONLY, 0);
            if (fd < 0) {
                // Try read-only for files without write permission
                fd = open(path, O_RDONLY, 0);
            }
            if (fd < 0) {
                err2("touch", path, "cannot open");
                status = 1;
                continue;
            }
            close(fd);
        }
    }

    return status;
}
