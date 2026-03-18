// cp -- copy files and directories (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -r/-R (recursive), -f (force)

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;

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

// Build dest path: dir/basename(src)
void build_dest(char* out, int outsz, const char* dir, const char* src) {
    // Find basename of src
    const char* base = src;
    for (const char* p = src; *p; ++p)
        if (*p == '/' && *(p + 1) != '\0') base = p + 1;

    int dlen = slen(dir);
    int blen = slen(base);
    int i = 0;
    for (int j = 0; j < dlen && i < outsz - 1; ++j) out[i++] = dir[j];
    if (i > 0 && out[i - 1] != '/' && i < outsz - 1) out[i++] = '/';
    for (int j = 0; j < blen && i < outsz - 1; ++j) out[i++] = base[j];
    out[i] = '\0';
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

int copy_file(const char* src, const char* dst, bool force) {
    struct stat st;
    if (stat(src, &st) < 0) return -1;

    if (force) unlink(dst);

    int fdi = open(src, O_RDONLY, 0);
    if (fdi < 0) return -1;

    int fdo = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
    if (fdo < 0) { close(fdi); return -1; }

    char buf[kBufSize];
    int status = 0;
    for (;;) {
        auto nr = read(fdi, buf, sizeof(buf));
        if (nr == 0) break;
        if (nr < 0) { status = -1; break; }
        const char* p = buf;
        auto left = nr;
        while (left > 0) {
            auto nw = write(fdo, p, static_cast<unsigned>(left));
            if (nw <= 0) { status = -1; break; }
            p += nw;
            left -= nw;
        }
        if (status != 0) break;
    }

    // Preserve permissions
    chmod(dst, st.st_mode & 07777);

    close(fdi);
    close(fdo);
    return status;
}

int copy_recursive(const char* src, const char* dst, bool force) {
    struct stat st;
    if (stat(src, &st) < 0) return -1;

    if (S_ISDIR(st.st_mode)) {
        mkdir(dst, st.st_mode & 07777);
        auto* d = opendir(src);
        if (!d) return -1;
        int status = 0;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_name[0] == '.' &&
                (ent->d_name[1] == '\0' ||
                 (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
                continue;
            char sp[1024], dp[1024];
            path_join(sp, sizeof(sp), src, ent->d_name);
            path_join(dp, sizeof(dp), dst, ent->d_name);
            if (copy_recursive(sp, dp, force) < 0) status = -1;
        }
        closedir(d);
        return status;
    }

    return copy_file(src, dst, force);
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
                    err("cp", "unknown option");
                    return 1;
                }
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    int nargs = argc - first_arg;
    if (nargs < 2) {
        err("cp", "missing operand");
        return 1;
    }

    const char* dest = argv[argc - 1];
    struct stat dst_st;
    bool dest_is_dir = (stat(dest, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));

    // Multiple sources require dest to be a directory
    if (nargs > 2 && !dest_is_dir) {
        err2("cp", dest, "not a directory");
        return 1;
    }

    int status = 0;
    for (int i = first_arg; i < argc - 1; ++i) {
        const char* src = argv[i];
        struct stat src_st;
        if (stat(src, &src_st) < 0) {
            err2("cp", src, "cannot stat");
            status = 1;
            continue;
        }

        if (S_ISDIR(src_st.st_mode) && !recursive) {
            err2("cp", src, "omitting directory (use -r)");
            status = 1;
            continue;
        }

        char target[1024];
        if (dest_is_dir) {
            build_dest(target, sizeof(target), dest, src);
        } else {
            int dlen = slen(dest);
            for (int j = 0; j < dlen && j < 1023; ++j) target[j] = dest[j];
            target[dlen < 1023 ? dlen : 1023] = '\0';
        }

        int r;
        if (recursive && S_ISDIR(src_st.st_mode))
            r = copy_recursive(src, target, force);
        else
            r = copy_file(src, target, force);

        if (r < 0) {
            err2("cp", src, "copy failed");
            status = 1;
        }
    }

    return status;
}
