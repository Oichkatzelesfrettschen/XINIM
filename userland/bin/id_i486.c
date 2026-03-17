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
    write(1, "uid=", 4);
    write_num(1, (unsigned long)getuid());
    write(1, "(root) gid=", 11);
    write_num(1, (unsigned long)getgid());
    write(1, "(root) euid=", 12);
    write_num(1, (unsigned long)geteuid());
    write(1, " egid=", 6);
    write_num(1, (unsigned long)getegid());
    write(1, "\n", 1);
    return 0;
}
