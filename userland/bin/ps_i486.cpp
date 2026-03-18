// ps -- report process status (XINIM)
// Cleanroom C++23 implementation.
// Uses XINIM SYS_procinfo syscall (100) to read the kernel process table.

#include <string.h>
#include <unistd.h>

namespace {

struct ProcInfoEntry {
    unsigned int pid;
    unsigned int ppid;
    unsigned int pgid;
    unsigned char state;
    unsigned char pad[3];
};

unsigned int sys_procinfo(void* buf, unsigned int size) {
    unsigned int result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(100),
          "b"(reinterpret_cast<unsigned int>(buf)),
          "c"(size)
        : "memory", "cc");
    return result;
}

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

void write_padded_num(int fd, unsigned long v, int width) {
    char buf[20];
    int pos = 0;
    if (v == 0) { buf[pos++] = '0'; }
    else {
        while (v > 0) {
            buf[pos++] = static_cast<char>('0' + v % 10);
            v /= 10;
        }
    }
    for (int i = pos; i < width; ++i)
        write_all(fd, " ", 1);
    for (int i = pos - 1; i >= 0; --i)
        write_all(fd, &buf[i], 1);
}

const char* state_name(unsigned char s) {
    switch (s) {
    case 0:  return "empty";
    case 1:  return "R";  // Runnable
    case 2:  return "S";  // Sleeping/Waiting
    case 3:  return "Z";  // Zombie/Exited
    default: return "?";
    }
}

} // namespace

int main() {
    ProcInfoEntry entries[16];
    auto count = sys_procinfo(entries, sizeof(entries));
    if (count > 16) count = 0;

    write_str(1, "  PID  PPID  PGID S\n");
    for (unsigned int i = 0; i < count; ++i) {
        write_padded_num(1, entries[i].pid, 5);
        write_str(1, " ");
        write_padded_num(1, entries[i].ppid, 5);
        write_str(1, " ");
        write_padded_num(1, entries[i].pgid, 5);
        write_str(1, " ");
        write_str(1, state_name(entries[i].state));
        write_str(1, "\n");
    }
    return 0;
}
