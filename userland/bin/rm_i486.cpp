// rm -- remove files and directories (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -r/-R (recursive), -f (force, no error on missing)

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
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

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

void path_join(char* out, int outsz, const char* dir, const char* name) {
    int dlen = slen(dir);
    int nlen = slen(name);
    int i = 0;
    for (int j = 0; j < dlen && i < outsz - 1; ++j) out[i++] = dir[j];
    if (i > 0 && out[i - 1] != '/' && i < outsz - 1) out[i++] = '/';
    for (int j = 0; j < nlen && i < outsz - 1; ++j) out[i++] = name[j];
    out[i] = '\0';
}

int remove_recursive(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0) return -1;

    if (S_ISDIR(st.st_mode)) {
        auto* d = opendir(path);
        if (!d) return -1;
        int status = 0;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_name[0] == '.' &&
                (ent->d_name[1] == '\0' ||
                 (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
                continue;
            char child[1024];
            path_join(child, sizeof(child), path, ent->d_name);
            if (remove_recursive(child) < 0) status = -1;
        }
        closedir(d);
        if (rmdir(path) < 0) status = -1;
        return status;
    }

    return unlink(path);
}

} // namespace

int main(int argc, char** argv) {
    bool recursive = false;
    bool force = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 'r': case 'R': recursive = true; break;
                case 'f': force = true; break;
                default:
                    write_str(2, "rm: unknown option\n");
                    return 1;
                }
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    if (first_arg >= argc) {
        if (force) return 0;
        write_str(2, "rm: missing operand\n");
        return 1;
    }

    int status = 0;
    for (int i = first_arg; i < argc; ++i) {
        const char* path = argv[i];
        struct stat st;

        if (stat(path, &st) < 0) {
            if (!force) {
                err2("rm", path, "no such file or directory");
                status = 1;
            }
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!recursive) {
                err2("rm", path, "is a directory (use -r)");
                status = 1;
                continue;
            }
            if (remove_recursive(path) < 0) {
                err2("rm", path, "removal failed");
                status = 1;
            }
        } else {
            if (unlink(path) < 0) {
                if (!force) {
                    err2("rm", path, "cannot remove");
                    status = 1;
                }
            }
        }
    }

    return status;
}
