// mv -- move (rename) files (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Tries rename() first; falls back to copy+unlink on EXDEV.

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

void path_join(char* out, int outsz, const char* dir, const char* name) {
    int dlen = slen(dir);
    int nlen = slen(name);
    int i = 0;
    for (int j = 0; j < dlen && i < outsz - 1; ++j) out[i++] = dir[j];
    if (i > 0 && out[i - 1] != '/' && i < outsz - 1) out[i++] = '/';
    for (int j = 0; j < nlen && i < outsz - 1; ++j) out[i++] = name[j];
    out[i] = '\0';
}

void build_dest(char* out, int outsz, const char* dir, const char* src) {
    const char* base = src;
    for (const char* p = src; *p; ++p)
        if (*p == '/' && *(p + 1) != '\0') base = p + 1;
    path_join(out, outsz, dir, base);
}

int copy_file(const char* src, const char* dst) {
    struct stat st;
    if (stat(src, &st) < 0) return -1;

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

    chmod(dst, st.st_mode & 07777);
    close(fdi);
    close(fdo);
    return status;
}

int copy_recursive(const char* src, const char* dst) {
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
            if (copy_recursive(sp, dp) < 0) status = -1;
        }
        closedir(d);
        return status;
    }

    return copy_file(src, dst);
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

// Move a single source to a resolved target path
int do_move(const char* src, const char* dst) {
    if (rename(src, dst) == 0) return 0;

    if (errno != EXDEV) return -1;

    // Cross-device: copy then remove original
    struct stat st;
    if (stat(src, &st) < 0) return -1;

    int r;
    if (S_ISDIR(st.st_mode))
        r = copy_recursive(src, dst);
    else
        r = copy_file(src, dst);

    if (r < 0) return -1;
    return remove_recursive(src);
}

} // namespace

int main(int argc, char** argv) {
    int first_arg = 1;

    // Skip options (mv has few; we accept -f for compatibility)
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            first_arg = i + 1;
        } else {
            break;
        }
    }

    int nargs = argc - first_arg;
    if (nargs < 2) {
        err("mv", "missing operand");
        return 1;
    }

    const char* dest = argv[argc - 1];
    struct stat dst_st;
    bool dest_is_dir = (stat(dest, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));

    if (nargs > 2 && !dest_is_dir) {
        err2("mv", dest, "not a directory");
        return 1;
    }

    int status = 0;
    for (int i = first_arg; i < argc - 1; ++i) {
        char target[1024];
        if (dest_is_dir)
            build_dest(target, sizeof(target), dest, argv[i]);
        else {
            int dlen = slen(dest);
            for (int j = 0; j < dlen && j < 1023; ++j) target[j] = dest[j];
            target[dlen < 1023 ? dlen : 1023] = '\0';
        }

        if (do_move(argv[i], target) < 0) {
            err2("mv", argv[i], "cannot move");
            status = 1;
        }
    }

    return status;
}
