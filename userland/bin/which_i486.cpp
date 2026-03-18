// which -- locate a command (common UNIX)
// Cleanroom C++23 implementation.
// Searches PATH environment variable for named executable.

#include <sys/stat.h>
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

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// Check if path is an executable file
bool is_executable(const char* path) {
    struct stat st{};
    if (stat(path, &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: which command ...\n");
        return 1;
    }

    // Get PATH from environment
    // environ is provided by libc
    extern char** environ;
    const char* path_env = nullptr;
    if (environ) {
        for (int i = 0; environ[i]; ++i) {
            const char* e = environ[i];
            if (e[0] == 'P' && e[1] == 'A' && e[2] == 'T' && e[3] == 'H' && e[4] == '=') {
                path_env = e + 5;
                break;
            }
        }
    }

    if (!path_env) path_env = "/bin:/usr/bin";

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        const char* name = argv[i];

        // If name contains /, check directly
        bool has_slash = false;
        for (int j = 0; name[j]; ++j) {
            if (name[j] == '/') { has_slash = true; break; }
        }
        if (has_slash) {
            if (is_executable(name)) {
                write_str(1, name);
                write_str(1, "\n");
            } else {
                status = 1;
            }
            continue;
        }

        // Search PATH
        bool found = false;
        const char* p = path_env;
        while (*p) {
            // Extract one directory from PATH
            char dir[512];
            int dlen = 0;
            while (*p && *p != ':' && dlen < static_cast<int>(sizeof(dir)) - 1)
                dir[dlen++] = *p++;
            if (*p == ':') ++p;
            dir[dlen] = '\0';
            if (dlen == 0) { dir[0] = '.'; dir[1] = '\0'; dlen = 1; }

            // Build full path: dir/name
            char full[1024];
            int nlen = slen(name);
            int fi = 0;
            for (int j = 0; j < dlen && fi < 1022; ++j) full[fi++] = dir[j];
            if (fi > 0 && full[fi - 1] != '/') full[fi++] = '/';
            for (int j = 0; j < nlen && fi < 1023; ++j) full[fi++] = name[j];
            full[fi] = '\0';

            if (is_executable(full)) {
                write_str(1, full);
                write_str(1, "\n");
                found = true;
                break;
            }
        }

        if (!found) status = 1;
    }

    return status;
}
