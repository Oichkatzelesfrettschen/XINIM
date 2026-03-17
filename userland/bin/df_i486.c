#include <sys/statfs.h>
#include <unistd.h>
#include <string.h>

static void write_num(int fd, unsigned long n) {
    char buf[20];
    int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = t;
    }
    write(fd, buf, pos);
}

int main(void) {
    struct statfs st;
    if (statfs("/", &st) != 0) {
        write(2, "df: statfs failed\n", 18);
        return 1;
    }
    unsigned long total_kb = (unsigned long)st.f_blocks * (unsigned long)st.f_bsize / 1024;
    unsigned long free_kb = (unsigned long)st.f_bfree * (unsigned long)st.f_bsize / 1024;
    unsigned long used_kb = total_kb - free_kb;

    write(1, "Filesystem     1K-blocks  Used  Available\n", 42);
    write(1, "/              ", 15);
    write_num(1, total_kb);
    write(1, "      ", 6);
    write_num(1, used_kb);
    write(1, "  ", 2);
    write_num(1, free_kb);
    write(1, "\n", 1);
    return 0;
}
