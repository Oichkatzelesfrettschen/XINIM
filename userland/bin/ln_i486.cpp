// ln -- make links (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -s (symbolic link)

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

} // namespace

int main(int argc, char** argv) {
    bool symbolic = false;
    int first_arg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; ++j) {
                switch (argv[i][j]) {
                case 's': symbolic = true; break;
                default:
                    err("ln", "unknown option");
                    return 1;
                }
            }
            first_arg = i + 1;
        } else {
            break;
        }
    }

    int nargs = argc - first_arg;
    if (nargs < 1) {
        err("ln", "missing operand");
        return 1;
    }

    // ln [-s] target (creates link in cwd with same basename)
    // ln [-s] target linkname
    // ln [-s] target... directory
    const char* dest = (nargs >= 2) ? argv[argc - 1] : nullptr;
    struct stat dst_st;
    bool dest_is_dir = dest && (stat(dest, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));

    if (nargs > 2 && !dest_is_dir) {
        err2("ln", dest, "not a directory");
        return 1;
    }

    int nsources = (nargs >= 2) ? nargs - 1 : 1;
    int status = 0;

    for (int i = first_arg; i < first_arg + nsources; ++i) {
        const char* source = argv[i];
        char target[1024];

        if (dest == nullptr) {
            // Single arg: create link in cwd with basename of source
            const char* base = source;
            for (const char* p = source; *p; ++p)
                if (*p == '/' && *(p + 1) != '\0') base = p + 1;
            int blen = slen(base);
            for (int j = 0; j < blen && j < 1023; ++j) target[j] = base[j];
            target[blen < 1023 ? blen : 1023] = '\0';
        } else if (dest_is_dir) {
            build_dest(target, sizeof(target), dest, source);
        } else {
            int dlen = slen(dest);
            for (int j = 0; j < dlen && j < 1023; ++j) target[j] = dest[j];
            target[dlen < 1023 ? dlen : 1023] = '\0';
        }

        int r;
        if (symbolic)
            r = symlink(source, target);
        else
            r = link(source, target);

        if (r < 0) {
            err2("ln", target, symbolic ? "cannot create symbolic link"
                                        : "cannot create hard link");
            status = 1;
        }
    }

    return status;
}
