#include <unistd.h>
#include <string.h>

/* Uses SYS_procinfo (100) to dump kernel process table */

struct ProcInfoEntry {
    unsigned int pid;
    unsigned int ppid;
    unsigned int pgid;
    unsigned char state;
    unsigned char pad[3];
};

static unsigned int sys_procinfo(void *buf, unsigned int size) {
    unsigned int result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(100), "b"((unsigned int)(unsigned long)buf), "c"(size)
        : "memory", "cc");
    return result;
}

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

static void write_padded_num(int fd, unsigned long n, int width) {
    char buf[20];
    int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = pos; i < width; ++i) write(fd, " ", 1);
    for (int i = pos - 1; i >= 0; --i) write(fd, buf + i, 1);
}

static const char *state_name(unsigned char s) {
    switch (s) {
    case 0: return "empty";
    case 1: return "R"; /* Runnable */
    case 2: return "S"; /* Sleeping/Waiting */
    case 3: return "Z"; /* Zombie/Exited */
    default: return "?";
    }
}

int main(void) {
    struct ProcInfoEntry entries[16];
    unsigned int count = sys_procinfo(entries, sizeof(entries));
    if (count > 16) count = 0; /* Error */

    write(1, "  PID  PPID  PGID S\n", 20);
    for (unsigned int i = 0; i < count; ++i) {
        write_padded_num(1, entries[i].pid, 5);
        write(1, " ", 1);
        write_padded_num(1, entries[i].ppid, 5);
        write(1, " ", 1);
        write_padded_num(1, entries[i].pgid, 5);
        write(1, " ", 1);
        const char *s = state_name(entries[i].state);
        write(1, s, strlen(s));
        write(1, "\n", 1);
    }
    (void)write_num;
    return 0;
}
