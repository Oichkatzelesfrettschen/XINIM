// id -- print real and effective user and group IDs (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.

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

void write_uint(int fd, unsigned v) {
    char tmp[12];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[12];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

void write_id_field(int fd, const char* label, unsigned id) {
    write_str(fd, label);
    write_uint(fd, id);
    write_str(fd, "(");
    if (id == 0) write_str(fd, "root");
    else write_uint(fd, id);
    write_str(fd, ")");
}

} // namespace

int main() {
    auto uid  = static_cast<unsigned>(getuid());
    auto gid  = static_cast<unsigned>(getgid());
    auto euid = static_cast<unsigned>(geteuid());
    auto egid = static_cast<unsigned>(getegid());

    write_id_field(1, "uid=", uid);
    write_str(1, " ");
    write_id_field(1, "gid=", gid);
    write_str(1, " ");
    write_str(1, "euid=");
    write_uint(1, euid);
    write_str(1, " ");
    write_str(1, "egid=");
    write_uint(1, egid);
    write_str(1, "\n");
    return 0;
}
