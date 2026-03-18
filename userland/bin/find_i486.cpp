// find -- search for files in a directory hierarchy (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports: -name PATTERN (glob * ?), -type f/d, -print (default),
//           -exec CMD {} \;

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr int kPathMax = 1024;
constexpr int kDentBuf = 2048;

// -- I/O helpers --

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

bool streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

// -- Path helpers --

int path_join(char* out, int outsz, const char* dir, const char* name) {
    int dlen = slen(dir);
    int nlen = slen(name);
    int need = dlen + 1 + nlen + 1;
    if (need > outsz) return -1;
    int i = 0;
    for (int j = 0; j < dlen; ++j) out[i++] = dir[j];
    if (i > 0 && out[i - 1] != '/') out[i++] = '/';
    for (int j = 0; j < nlen; ++j) out[i++] = name[j];
    out[i] = '\0';
    return 0;
}

const char* basename_of(const char* path) {
    const char* p = path;
    const char* last = path;
    while (*p) {
        if (*p == '/' && *(p + 1) != '\0') last = p + 1;
        ++p;
    }
    return last;
}

// -- Glob matching --

bool glob_match(const char* name, const char* pat) {
    while (*pat) {
        if (*pat == '*') {
            ++pat;
            if (*pat == '\0') return true;
            while (*name) {
                if (glob_match(name, pat)) return true;
                ++name;
            }
            return false;
        } else if (*pat == '?') {
            if (*name == '\0') return false;
            ++pat;
            ++name;
        } else {
            if (*name != *pat) return false;
            ++pat;
            ++name;
        }
    }
    return *name == '\0';
}

// -- getdents syscall --

struct linux_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[1];
};

long sys_getdents(int fd, void* buf, unsigned count) {
    long result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(141), "b"(fd), "c"(buf), "d"(count)
        : "memory", "cc");
    return result;
}

// -- exec support --

// Fork/exec for -exec CMD {} \;
// Linux i486 syscalls: fork=2, waitpid=7, execve=11
long sys_fork() {
    long result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(2)
        : "memory", "cc");
    return result;
}

long sys_waitpid(int pid, int* status, int options) {
    long result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(7), "b"(pid), "c"(status), "d"(options)
        : "memory", "cc");
    return result;
}

// -- Filter state --

int g_type_filter;          // 0=any, 'f'=file, 'd'=dir
const char* g_name_pattern; // null = no filter
bool g_has_exec;
int g_exec_argc;
char** g_exec_argv;         // points into argv; {} replaced per-match
int g_exec_brace_idx;       // index of {} in exec argv, or -1

void run_exec(const char* path) {
    if (!g_has_exec) return;

    // Replace {} placeholder with path
    char* saved = nullptr;
    if (g_exec_brace_idx >= 0) {
        saved = g_exec_argv[g_exec_brace_idx];
        g_exec_argv[g_exec_brace_idx] = const_cast<char*>(path);
    }

    long pid = sys_fork();
    if (pid == 0) {
        // child
        execve(g_exec_argv[0], g_exec_argv, nullptr);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        sys_waitpid(static_cast<int>(pid), &status, 0);
    }

    if (g_exec_brace_idx >= 0) {
        g_exec_argv[g_exec_brace_idx] = saved;
    }
}

// -- Recursive walk --

void find_recursive(const char* path) {
    struct stat st{};
    if (stat(path, &st) != 0) return;

    bool is_dir = S_ISDIR(st.st_mode);
    bool is_file = S_ISREG(st.st_mode);

    // Apply filters
    bool show = true;
    if (g_type_filter == 'f' && !is_file) show = false;
    if (g_type_filter == 'd' && !is_dir) show = false;
    if (g_name_pattern && !glob_match(basename_of(path), g_name_pattern))
        show = false;

    if (show) {
        if (g_has_exec) {
            run_exec(path);
        } else {
            write_str(1, path);
            write_str(1, "\n");
        }
    }

    if (!is_dir) return;

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) return;

    char dbuf[kDentBuf];
    for (;;) {
        auto nread = sys_getdents(fd, dbuf, sizeof(dbuf));
        if (nread <= 0) break;

        int pos = 0;
        while (pos < static_cast<int>(nread)) {
            auto* ent = reinterpret_cast<linux_dirent*>(dbuf + pos);
            const char* name = ent->d_name;

            // Skip . and ..
            if (name[0] == '.' &&
                (name[1] == '\0' ||
                 (name[1] == '.' && name[2] == '\0'))) {
                pos += ent->d_reclen;
                continue;
            }

            char child[kPathMax];
            if (path_join(child, static_cast<int>(sizeof(child)),
                          path, name) == 0) {
                find_recursive(child);
            }
            pos += ent->d_reclen;
        }
    }
    close(fd);
}

} // namespace

int main(int argc, char** argv) {
    const char* start_path = ".";
    int argi = 1;

    // First non-option argument is the starting path
    if (argi < argc && argv[argi][0] != '-') {
        start_path = argv[argi++];
    }

    // Parse predicates
    while (argi < argc) {
        if (streq(argv[argi], "-name") && argi + 1 < argc) {
            g_name_pattern = argv[++argi];
        } else if (streq(argv[argi], "-type") && argi + 1 < argc) {
            g_type_filter = argv[++argi][0];
        } else if (streq(argv[argi], "-print")) {
            // default action, no-op
        } else if (streq(argv[argi], "-exec") && argi + 1 < argc) {
            g_has_exec = true;
            ++argi;
            // Count args until ";"
            int start = argi;
            int count = 0;
            g_exec_brace_idx = -1;
            while (argi < argc && !streq(argv[argi], ";")) {
                if (streq(argv[argi], "{}")) {
                    g_exec_brace_idx = count;
                }
                ++count;
                ++argi;
            }
            // Build exec argv (null-terminated)
            // We reuse the original argv pointers
            static char* exec_argv[64];
            g_exec_argc = count;
            for (int i = 0; i < count && i < 63; ++i) {
                exec_argv[i] = argv[start + i];
            }
            exec_argv[count < 63 ? count : 63] = nullptr;
            g_exec_argv = exec_argv;
        }
        ++argi;
    }

    find_recursive(start_path);
    return 0;
}
