// kill -- send signals to processes (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

#include <signal.h>
#include <string.h>
#include <unistd.h>

namespace {

struct SigEntry {
    const char* name;
    int num;
};

constexpr SigEntry kSignals[] = {
    {"HUP",  1},  {"INT",  2},  {"QUIT", 3},  {"ILL",  4},
    {"TRAP", 5},  {"ABRT", 6},  {"BUS",  7},  {"FPE",  8},
    {"KILL", 9},  {"USR1", 10}, {"SEGV", 11}, {"USR2", 12},
    {"PIPE", 13}, {"ALRM", 14}, {"TERM", 15}, {"CHLD", 17},
    {"CONT", 18}, {"STOP", 19}, {"TSTP", 20},
    {nullptr, 0}
};

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

void write_uint(int fd, unsigned v) {
    char tmp[12];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[12];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

long parse_long(const char* s) {
    long v = 0;
    bool neg = false;
    if (*s == '-') { neg = true; ++s; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return neg ? -v : v;
}

int lookup_signal(const char* name) {
    for (int i = 0; kSignals[i].name != nullptr; ++i) {
        if (strcmp(name, kSignals[i].name) == 0) return kSignals[i].num;
    }
    // Try with SIG prefix stripped
    if (name[0] == 'S' && name[1] == 'I' && name[2] == 'G') {
        return lookup_signal(name + 3);
    }
    // Try numeric
    if (name[0] >= '0' && name[0] <= '9')
        return static_cast<int>(parse_long(name));
    return -1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: kill [-signal | -s signal] pid ...\n");
        return 1;
    }

    int sig = 15; // SIGTERM
    int argi = 1;

    if (argv[1][0] == '-') {
        if (argv[1][1] == 'l') {
            // List signals
            for (int i = 0; kSignals[i].name != nullptr; ++i) {
                write_uint(1, static_cast<unsigned>(kSignals[i].num));
                write_str(1, ") ");
                write_str(1, kSignals[i].name);
                write_str(1, "\n");
            }
            return 0;
        }
        if (argv[1][1] == 's') {
            // -s SIGNAL form
            if (argc < 3) {
                write_str(2, "kill: -s requires signal name\n");
                return 1;
            }
            sig = lookup_signal(argv[2]);
            argi = 3;
        } else {
            // -SIGNAL form
            sig = lookup_signal(argv[1] + 1);
            argi = 2;
        }
        if (sig < 0) {
            write_str(2, "kill: unknown signal\n");
            return 1;
        }
    }

    if (argi >= argc) {
        write_str(2, "kill: no pid specified\n");
        return 1;
    }

    int status = 0;
    for (int i = argi; i < argc; ++i) {
        auto pid = static_cast<int>(parse_long(argv[i]));
        if (kill(pid, sig) != 0) {
            write_str(2, "kill: failed for pid ");
            write_str(2, argv[i]);
            write_str(2, "\n");
            status = 1;
        }
    }
    return status;
}
