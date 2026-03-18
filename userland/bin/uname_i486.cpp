// uname -- print system information (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -s (sysname), -n (nodename), -r (release), -m (machine), -a (all)

#include <sys/utsname.h>
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

} // namespace

int main(int argc, char** argv) {
    struct utsname u{};
    if (uname(&u) != 0) {
        write_str(2, "uname: syscall failed\n");
        return 1;
    }

    bool show_s = false;
    bool show_n = false;
    bool show_r = false;
    bool show_m = false;

    for (int i = 1; i < argc; ++i) {
        for (const char* p = argv[i]; *p != '\0'; ++p) {
            if (*p == '-') continue;
            switch (*p) {
            case 'a': show_s = show_n = show_r = show_m = true; break;
            case 's': show_s = true; break;
            case 'n': show_n = true; break;
            case 'r': show_r = true; break;
            case 'm': show_m = true; break;
            default: break;
            }
        }
    }

    // Default: -s
    if (!show_s && !show_n && !show_r && !show_m) show_s = true;

    bool first = true;
    auto emit = [&](const char* val) {
        if (!first) write_str(1, " ");
        write_str(1, val);
        first = false;
    };

    if (show_s) emit(u.sysname);
    if (show_n) emit(u.nodename);
    if (show_r) emit(u.release);
    if (show_m) emit(u.machine);
    write_str(1, "\n");
    return 0;
}
