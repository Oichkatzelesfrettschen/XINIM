// df -- report filesystem disk space usage (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Uses statfs() on root filesystem.

#include <sys/statfs.h>
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

void write_uint(int fd, unsigned long v) {
    char tmp[20];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[20];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

} // namespace

int main() {
    struct statfs st{};
    if (statfs("/", &st) != 0) {
        write_str(2, "df: statfs failed\n");
        return 1;
    }

    auto bsize = static_cast<unsigned long>(st.f_bsize);
    auto total_kb = static_cast<unsigned long>(st.f_blocks) * bsize / 1024;
    auto free_kb  = static_cast<unsigned long>(st.f_bfree)  * bsize / 1024;
    auto used_kb  = total_kb - free_kb;

    write_str(1, "Filesystem     1K-blocks  Used  Available\n");
    write_str(1, "/              ");
    write_uint(1, total_kb);
    write_str(1, "      ");
    write_uint(1, used_kb);
    write_str(1, "  ");
    write_uint(1, free_kb);
    write_str(1, "\n");
    return 0;
}
