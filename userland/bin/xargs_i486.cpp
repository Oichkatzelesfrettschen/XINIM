// xargs -- build and execute command lines from stdin (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports: -n MAX_ARGS. Default command is echo.

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

bool streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// Linux i486 syscalls
long sys_fork() {
    long result;
    __asm__ volatile("int $0x80" : "=a"(result) : "a"(2) : "memory", "cc");
    return result;
}

long sys_waitpid(int pid, int* status, int options) {
    long result;
    __asm__ volatile("int $0x80" : "=a"(result)
        : "a"(7), "b"(pid), "c"(status), "d"(options) : "memory", "cc");
    return result;
}

// Read one line from stdin into buf. Returns length, 0 on EOF, -1 on error.
int read_line(char* buf, int bufsz) {
    int n = 0;
    while (n < bufsz - 1) {
        char c;
        auto r = read(0, &c, 1);
        if (r <= 0) return n;  // EOF or error
        if (c == '\n') break;
        buf[n++] = c;
    }
    buf[n] = '\0';
    return n;
}

void run_command(char** argv) {
    long pid = sys_fork();
    if (pid == 0) {
        execve(argv[0], argv, nullptr);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        sys_waitpid(static_cast<int>(pid), &status, 0);
    }
}

} // namespace

int main(int argc, char** argv) {
    int max_args = 0;  // 0 = unlimited (all at once)
    int cmd_start = 1;

    // Parse options
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && argv[i][2] == '\0') {
            if (i + 1 < argc) {
                ++i;
                max_args = 0;
                for (int j = 0; argv[i][j] != '\0'; ++j)
                    max_args = max_args * 10 + (argv[i][j] - '0');
                cmd_start = i + 1;
            }
        } else {
            cmd_start = i;
            break;
        }
    }

    // Default command: /bin/echo
    const char* default_cmd = "/bin/echo";
    char** cmd_argv = argv + cmd_start;
    int cmd_argc = argc - cmd_start;
    bool using_default = (cmd_argc == 0);

    // Read all lines from stdin
    constexpr int kMaxLines = 4096;
    constexpr int kLineBuf = 256;
    static char line_storage[kMaxLines][kLineBuf];
    int line_count = 0;

    while (line_count < kMaxLines) {
        int n = read_line(line_storage[line_count], kLineBuf);
        if (n <= 0) {
            // Check if we actually got nothing (EOF with no data)
            if (n == 0 && line_storage[line_count][0] == '\0') break;
            if (n == 0) { ++line_count; break; }
            break;
        }
        ++line_count;
    }

    if (line_count == 0) return 0;

    // Build and execute commands
    int batch = (max_args > 0) ? max_args : line_count;
    int base_argc = using_default ? 1 : cmd_argc;

    for (int i = 0; i < line_count; i += batch) {
        int count = batch;
        if (i + count > line_count) count = line_count - i;

        // Build argv: command args + batch of lines + null
        constexpr int kMaxArgv = 4128;
        static char* exec_argv[kMaxArgv];
        int idx = 0;

        if (using_default) {
            exec_argv[idx++] = const_cast<char*>(default_cmd);
        } else {
            for (int j = 0; j < cmd_argc && idx < kMaxArgv - 1; ++j)
                exec_argv[idx++] = cmd_argv[j];
        }

        for (int j = 0; j < count && idx < kMaxArgv - 1; ++j)
            exec_argv[idx++] = line_storage[i + j];

        exec_argv[idx] = nullptr;
        run_command(exec_argv);
    }

    return 0;
}
