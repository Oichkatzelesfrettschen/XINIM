// mkdir -- make directories (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -p (create parent directories as needed)

#include <errno.h>
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

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// Create directory and all parent components.
// Returns 0 on success, -1 on failure.
int mkdir_parents(const char* path) {
    char tmp[1024];
    int len = slen(path);
    if (len >= static_cast<int>(sizeof(tmp))) return -1;

    for (int i = 0; i < len; ++i) tmp[i] = path[i];
    tmp[len] = '\0';

    // Walk through path components and create each
    for (int i = 1; i <= len; ++i) {
        if (i == len || tmp[i] == '/') {
            char saved = tmp[i];
            tmp[i] = '\0';

            struct stat st;
            if (stat(tmp, &st) < 0) {
                if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
                    return -1;
            } else if (!S_ISDIR(st.st_mode)) {
                return -1;
            }

            tmp[i] = saved;
        }
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    bool parents = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 'p': parents = true; break;
                default:
                    err("mkdir", "unknown option");
                    return 1;
                }
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    if (first_arg >= argc) {
        err("mkdir", "missing operand");
        return 1;
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        if (parents) {
            if (mkdir_parents(argv[i]) < 0) {
                err2("mkdir", argv[i], "cannot create directory");
                status = 1;
            }
        } else {
            if (mkdir(argv[i], 0755) < 0) {
                err2("mkdir", argv[i], "cannot create directory");
                status = 1;
            }
        }
    }

    return status;
}
